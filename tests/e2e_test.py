#!/usr/bin/env python3
"""
End-to-End Integration Test: Company ↔ Kernel ↔ Game

Tests the complete flow:
  1. Start kernel daemon
  2. Company creates agent and grants skills/XP via Python IPC client
  3. Game reads agent state from kernel and verifies
  4. Skill mapping verification (backend_dev → 阵法造诣)
  5. Multi-agent support
  6. Cleanup
"""

import json
import os
import signal
import socket
import subprocess
import sys
import time

# ── paths ────────────────────────────────────────────────────────────
SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
KERNEL_ROOT = os.path.dirname(SCRIPT_DIR)
DAEMON_BIN = os.path.join(KERNEL_ROOT, "build", "agent-kernel-daemon")
SKILL_MAPPING = os.path.join(KERNEL_ROOT, "config", "skill-mapping.json")
SOCKET_PATH = "/tmp/agent-kernel-e2e-test.sock"

# Add the Python client to path (MDH/backend)
MDH_BACKEND = os.path.join(os.path.dirname(os.path.dirname(KERNEL_ROOT)), "MDH", "backend")
sys.path.insert(0, MDH_BACKEND)

from agent_kernel_client import AgentKernelClient, KernelAgent


# ── helpers ──────────────────────────────────────────────────────────

class TestFailure(Exception):
    pass


def check(condition: bool, msg: str):
    if not condition:
        raise TestFailure(f"CHECK FAILED: {msg}")
    print(f"    ✓ {msg}")


def wait_for_socket(path: str, timeout: float = 5.0):
    """Wait until a Unix socket is accepting connections."""
    deadline = time.time() + timeout
    while time.time() < deadline:
        try:
            s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
            s.connect(path)
            s.close()
            return
        except (ConnectionRefusedError, FileNotFoundError, OSError):
            time.sleep(0.1)
    raise RuntimeError(f"Socket {path} not ready after {timeout}s")


def load_skill_mapping() -> dict:
    with open(SKILL_MAPPING, "r", encoding="utf-8") as f:
        return json.load(f)


# ── test steps ───────────────────────────────────────────────────────

def step1_start_daemon() -> subprocess.Popen:
    print("[1/6] Starting kernel daemon...")
    if os.path.exists(SOCKET_PATH):
        os.unlink(SOCKET_PATH)
    proc = subprocess.Popen(
        [DAEMON_BIN, "--socket", SOCKET_PATH],
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
    )
    wait_for_socket(SOCKET_PATH)
    print(f"    Daemon started (pid={proc.pid}, socket={SOCKET_PATH})")
    return proc


def step2_create_agent(client: AgentKernelClient) -> KernelAgent:
    print('[2/6] Company: Creating agent "小明"...')
    agent = client.create_agent(
        name="小明",
        department="engineering",
        company_role="backend_dev",
        agent_id="xiaoming-001",
    )
    check(agent.name == "小明", f'Agent name is "小明" (got "{agent.name}")')
    check(agent.department == "engineering", f'Department is "engineering" (got "{agent.department}")')
    check(agent.company_role == "backend_dev", f'companyRole is "backend_dev" (got "{agent.company_role}")')
    check(agent.entity_id >= 0, f"entity_id is valid ({agent.entity_id})")
    check(agent.total_xp == 0, f"Initial career XP is 0 (got {agent.total_xp})")
    check(agent.career_stage == "Junior", f'Initial career stage is "Junior" (got "{agent.career_stage}")')
    print(f"    Agent created: entity_id={agent.entity_id}, id={agent.id}")
    return agent


def step3_add_skills_and_xp(client: AgentKernelClient, entity_id: int):
    print("[3/6] Company: Adding skills and XP...")

    # Add skills to the skill tree
    client.add_skill(entity_id, "backend_dev", "Engineering")
    print('    Added skill: backend_dev (Engineering)')

    client.add_skill(entity_id, "testing", "Engineering")
    print('    Added skill: testing (Engineering)')

    # Add skill XP
    client.add_skill_xp(entity_id, "backend_dev", 100)
    print("    backend_dev: +100 XP")

    client.add_skill_xp(entity_id, "testing", 50)
    print("    testing: +50 XP")

    # Grant career XP (600 → should promote Junior→Mid, threshold is 500)
    client.add_career_xp(entity_id, 600)
    print("    Career: +600 XP")

    # Verify skill tree
    skills = client.get_skills(entity_id)
    check("backend_dev" in skills, "Skill tree contains backend_dev")
    check("testing" in skills, "Skill tree contains testing")
    check(skills["backend_dev"]["xp"] == 100, f'backend_dev XP is 100 (got {skills["backend_dev"]["xp"]})')
    check(skills["testing"]["xp"] == 50, f'testing XP is 50 (got {skills["testing"]["xp"]})')
    print("    Skills verified in kernel")


def step4_verify_agent_state(client: AgentKernelClient, entity_id: int):
    print("[4/6] Game: Verifying agent state...")

    # Get agent from kernel (simulating Game reading)
    agent = client.get_agent(entity_id)
    check(agent is not None, "Agent retrieved from kernel")

    # Identity
    check(agent.name == "小明", f'Name matches: "小明" (got "{agent.name}")')
    check(agent.department == "engineering", f'Department matches (got "{agent.department}")')
    check(agent.company_role == "backend_dev", f'companyRole matches (got "{agent.company_role}")')

    # Skill tree
    check("backend_dev" in agent.skills, "Skill tree has backend_dev")
    check("testing" in agent.skills, "Skill tree has testing")

    backend_skill = agent.skills["backend_dev"]
    check(backend_skill["xp"] == 100, f'backend_dev XP = 100 (got {backend_skill["xp"]})')
    testing_skill = agent.skills["testing"]
    check(testing_skill["xp"] == 50, f'testing XP = 50 (got {testing_skill["xp"]})')

    # Career stage promotion (600xp >=500 threshold → Mid)
    check(agent.total_xp == 600, f"Career totalXp = 600 (got {agent.total_xp})")
    check(agent.career_stage == "Mid", f'Career stage = "Mid" (got "{agent.career_stage}")')
    print("    All identity, skill, and career fields verified")


def step5_verify_skill_mapping():
    print("[5/6] Verifying skill mapping...")
    mapping = load_skill_mapping()

    # backend_dev → 阵法造诣
    check("backend_dev" in mapping, "skill-mapping.json contains backend_dev")
    check(
        mapping["backend_dev"]["gameAbility"] == "阵法造诣",
        f'backend_dev maps to "阵法造诣" (got "{mapping["backend_dev"]["gameAbility"]}")',
    )

    # testing → 试炼阵法
    check("testing" in mapping, "skill-mapping.json contains testing")
    check(
        mapping["testing"]["gameAbility"] == "试炼阵法",
        f'testing maps to "试炼阵法" (got "{mapping["testing"]["gameAbility"]}")',
    )

    # Verify categories match
    check(mapping["backend_dev"]["category"] == "Engineering", "backend_dev category is Engineering")
    check(mapping["testing"]["category"] == "Engineering", "testing category is Engineering")

    print("    Skill mapping verified (Company skill → Game ability)")


def step6_create_second_agent(client: AgentKernelClient) -> KernelAgent:
    print("[6/6] Creating second agent...")
    agent2 = client.create_agent(
        name="小红",
        department="design",
        company_role="graphic_designer",
        agent_id="xiaohong-002",
    )
    check(agent2.name == "小红", f'Second agent name is "小红" (got "{agent2.name}")')
    check(agent2.entity_id >= 0, f"Second agent entity_id valid ({agent2.entity_id})")

    # Verify both agents exist
    agents = client.list_agents()
    check(len(agents) == 2, f"Kernel has 2 agents (got {len(agents)})")

    names = {a.name for a in agents}
    check("小明" in names, "Agent list contains 小明")
    check("小红" in names, "Agent list contains 小红")
    print(f"    Both agents verified: {names}")
    return agent2


def cleanup(proc: subprocess.Popen, client: AgentKernelClient):
    print("\nCleanup...")
    if client and client.is_connected:
        client.disconnect()
        print("    Client disconnected")
    if proc:
        proc.send_signal(signal.SIGTERM)
        proc.wait(timeout=5)
        print(f"    Daemon stopped (exit={proc.returncode})")
    if os.path.exists(SOCKET_PATH):
        os.unlink(SOCKET_PATH)
        print("    Socket removed")


# ── main ─────────────────────────────────────────────────────────────

def main():
    print("=== MDH 大荒界 E2E Integration Test ===\n")

    proc = None
    client = AgentKernelClient(SOCKET_PATH)

    try:
        # Step 1: Start daemon
        proc = step1_start_daemon()
        client.connect()
        print("    Client connected\n")

        # Step 2: Create agent
        agent = step2_create_agent(client)
        print()

        # Step 3: Add skills and XP
        step3_add_skills_and_xp(client, agent.entity_id)
        print()

        # Step 4: Game verifies agent state
        step4_verify_agent_state(client, agent.entity_id)
        print()

        # Step 5: Skill mapping
        step5_verify_skill_mapping()
        print()

        # Step 6: Second agent + multi-agent check
        step6_create_second_agent(client)
        print()

        # ── All passed ──────────────────────────────────────────────
        print("=" * 50)
        print("=== ALL E2E TESTS PASSED ===")
        print("=" * 50)
        print("  - Agent created in kernel via Company client ✓")
        print("  - Skills added (backend_dev, testing) ✓")
        print("  - Career XP granted, stage promoted ✓")
        print("  - Game can read agent state ✓")
        print("  - Skill mapping verified ✓")
        print("  - Multiple agents supported ✓")

    except TestFailure as e:
        print(f"\n✗ TEST FAILED: {e}", file=sys.stderr)
        cleanup(proc, client)
        sys.exit(1)
    except Exception as e:
        print(f"\n✗ UNEXPECTED ERROR: {e}", file=sys.stderr)
        cleanup(proc, client)
        sys.exit(1)

    cleanup(proc, client)


if __name__ == "__main__":
    main()
