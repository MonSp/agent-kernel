#pragma once

#include "../Component.h"
#include <cstdint>
#include <cstring>
#include <algorithm>

#pragma pack(push, 1)
struct InteractionSlot {
    uint64_t timestamp;
    uint32_t otherSlot;
    uint8_t  type;
    int8_t   impactScore;
};
#pragma pack(pop)

#pragma pack(push, 1)
struct WitnessedSlot {
    uint64_t timestamp;
    uint32_t slot;
    uint8_t  significance;
    uint8_t  _pad;
};
#pragma pack(pop)

#pragma pack(push, 1)
struct CommandMemorySlot {
    uint64_t timestamp;
    uint32_t issuerSlot;
    uint32_t commandId;
    uint8_t  result;
    uint8_t  emotionTag;
    int8_t   influence;
    uint8_t  _pad;
};
#pragma pack(pop)

enum class RumorSeverity : uint8_t {
    Tribulation = 10,
    Assassination = 9,
    Embezzlement = 8,
    ClanWar = 7,
    DaoBonding = 6,
    Death = 5,
    DuelOutcome = 4,
    ResourceDispute = 3,
    DailyConflict = 2,
    GossipChatter = 1
};

#pragma pack(push, 1)
struct RumorPacket {
    uint64_t timestamp = 0;
    uint32_t originalEventSlot = 0;
    uint32_t originalWitness = 0;
    int8_t   contentIntegrity = 0;
    uint8_t  hopCount = 0;
    uint8_t  sensitivity = 0;
    RumorSeverity severity = RumorSeverity::GossipChatter;
    uint64_t queuedSinceFrame = 0;
    uint64_t bornFrame = 0;
};
#pragma pack(pop)

enum class MilestoneType : uint8_t {
    None = 0,
    BreakthroughRealm = 1,
    DaoCompanionBond = 2,
    LifeDeathBattle = 3,
    ClanWar = 4,
    MajorCommand = 5,
    ExpelledFromSect = 6
};

#pragma pack(push, 1)
struct MidTermSummary {
    uint32_t targetSlot;
    uint16_t interactionCount;
    int8_t   avgEmotionScore;
    uint64_t firstTime;
    uint64_t lastTime;
    uint8_t  category;
};
#pragma pack(pop)

#pragma pack(push, 1)
struct LongTermMilestone {
    uint64_t timestamp;
    MilestoneType type;
    uint32_t relatedSlot;
    uint8_t  significance;
};
#pragma pack(pop)

template<typename T, size_t CAPACITY>
class RingBuffer {
    T data[CAPACITY];
    size_t head;
    size_t count;
public:
    RingBuffer() : head(0), count(0) { data[0] = T{}; }

    void push(const T& item) {
        data[head] = item;
        head = (head + 1) % CAPACITY;
        if (count < CAPACITY) count++;
    }

    size_t size() const { return count; }
    bool empty() const { return count == 0; }

    size_t getRecent(T* out, size_t n) const {
        if (n > count) n = count;
        for (size_t i = 0; i < n; i++) {
            size_t idx = (head + CAPACITY - 1 - i) % CAPACITY;
            out[i] = data[idx];
        }
        return n;
    }

    const T* rawData() const { return data; }
};

struct MemoryRingComponent : public ECS::ComponentBase<MemoryRingComponent> {
    static constexpr size_t MAX_RECENT_INTERACTIONS = 20;
    static constexpr size_t MAX_RECENT_WITNESSED = 30;
    static constexpr size_t MAX_RECENT_COMMAND = 30;
    static constexpr size_t MAX_RUMORS = 20;
    static constexpr size_t MAX_MIDTERM = 100;
    static constexpr size_t MAX_LONGTERM = 50;
    static constexpr uint64_t MAX_RUMOR_TTL = 900;
    static constexpr size_t MAX_RUMOR_QUEUE = 500;

    RingBuffer<InteractionSlot, MAX_RECENT_INTERACTIONS> interactions;
    RingBuffer<WitnessedSlot, MAX_RECENT_WITNESSED> witnessed;
    RingBuffer<CommandMemorySlot, MAX_RECENT_COMMAND> commandMemory;
    RingBuffer<RumorPacket, MAX_RUMORS> rumors;
    RingBuffer<MidTermSummary, MAX_MIDTERM> midTerm;
    RingBuffer<LongTermMilestone, MAX_LONGTERM> longTerm;

    MemoryRingComponent() = default;

    static RumorSeverity significanceToSeverity(uint8_t significance) {
        switch (significance) {
            case 10: return RumorSeverity::Tribulation;
            case 9:  return RumorSeverity::Assassination;
            case 8:  return RumorSeverity::Embezzlement;
            case 7:  return RumorSeverity::ClanWar;
            case 6:  return RumorSeverity::DaoBonding;
            case 5:  return RumorSeverity::Death;
            case 4:  return RumorSeverity::DuelOutcome;
            case 3:  return RumorSeverity::ResourceDispute;
            case 2:  return RumorSeverity::DailyConflict;
            default: return RumorSeverity::GossipChatter;
        }
    }

    bool startRumor(uint32_t witnessSlot, uint32_t eventSlot, uint8_t sensitivity, uint64_t currentFrame) {
        if (rumors.size() >= MAX_RUMOR_QUEUE) {
            evictLowestSeverityRumor();
        }
        RumorPacket rumor;
        rumor.timestamp = 0;
        rumor.originalEventSlot = eventSlot;
        rumor.originalWitness = witnessSlot;
        rumor.contentIntegrity = 100;
        rumor.hopCount = 0;
        rumor.sensitivity = sensitivity;
        rumor.severity = significanceToSeverity(sensitivity);
        rumor.queuedSinceFrame = 0;
        rumor.bornFrame = currentFrame;
        rumors.push(rumor);
        return true;
    }

    bool receiveRumor(const RumorPacket& incoming, uint32_t newWitness) {
        if (rumors.size() >= MAX_RUMOR_QUEUE) {
            evictLowestSeverityRumor();
        }
        RumorPacket mutated = incoming;
        mutated.hopCount++;
        int8_t decay = 15 + (mutated.sensitivity / 3);
        mutated.contentIntegrity -= decay;
        if (mutated.contentIntegrity < 0) mutated.contentIntegrity = 0;
        mutated.originalWitness = newWitness;
        mutated.bornFrame = incoming.bornFrame;
        rumors.push(mutated);
        return true;
    }

    bool knowsRumor(uint32_t eventSlot) const {
        size_t n = rumors.size();
        const RumorPacket* data = rumors.rawData();
        for (size_t i = 0; i < n; i++) {
            if (data[i].originalEventSlot == eventSlot) return true;
        }
        return false;
    }

    void evictLowestSeverityRumor() {
        RumorPacket buf[MAX_RUMORS];
        size_t n = rumors.size();
        if (n == 0) return;
        rumors.getRecent(buf, n);
        size_t lowestIdx = 0;
        uint8_t lowestSev = static_cast<uint8_t>(buf[0].severity);
        for (size_t i = 1; i < n; i++) {
            uint8_t sev = static_cast<uint8_t>(buf[i].severity);
            if (sev < lowestSev) { lowestSev = sev; lowestIdx = i; }
        }
        RingBuffer<RumorPacket, MAX_RUMORS> fresh;
        for (size_t i = 0; i < n; i++) {
            if (i != lowestIdx) fresh.push(buf[i]);
        }
        rumors = fresh;
    }

    void cleanExpiredRumors(uint64_t currentFrame) {
        RumorPacket buf[MAX_RUMORS];
        size_t n = rumors.size();
        if (n == 0) return;
        rumors.getRecent(buf, n);
        RingBuffer<RumorPacket, MAX_RUMORS> fresh;
        for (size_t i = 0; i < n; i++) {
            if (currentFrame - buf[i].bornFrame < MAX_RUMOR_TTL) {
                fresh.push(buf[i]);
            }
        }
        rumors = fresh;
    }

    void recordMilestone(MilestoneType type, uint32_t relatedSlot, uint8_t significance) {
        LongTermMilestone m;
        m.timestamp = 0;
        m.type = type;
        m.relatedSlot = relatedSlot;
        m.significance = significance;
        longTerm.push(m);
    }

    void upgradeMidTermToLongTerm(uint64_t currentFrame) {
        MidTermSummary all[MAX_MIDTERM];
        size_t n = midTerm.getRecent(all, MAX_MIDTERM);
        RingBuffer<MidTermSummary, MAX_MIDTERM> retained;

        for (size_t i = 0; i < n; i++) {
            int absScore = all[i].avgEmotionScore < 0 ? -all[i].avgEmotionScore : all[i].avgEmotionScore;
            if (absScore >= 80 && (currentFrame - all[i].firstTime) >= 1000) {
                LongTermMilestone m;
                m.timestamp = currentFrame;
                m.relatedSlot = all[i].targetSlot;
                m.significance = static_cast<uint8_t>(absScore > 127 ? 127 : absScore);

                if (all[i].category == 2) {
                    m.type = MilestoneType::MajorCommand;
                } else if (all[i].category == 1) {
                    m.type = (all[i].avgEmotionScore >= 0) ? MilestoneType::ClanWar : MilestoneType::LifeDeathBattle;
                } else {
                    m.type = (all[i].avgEmotionScore >= 0) ? MilestoneType::DaoCompanionBond : MilestoneType::ExpelledFromSect;
                }
                longTerm.push(m);
            } else {
                retained.push(all[i]);
            }
        }
        midTerm = retained;
    }

    void tryAutoCompact(uint64_t currentFrame) {
        if (interactions.size() >= MAX_RECENT_INTERACTIONS) {
            compressToMidTerm(currentFrame);
            interactions = RingBuffer<InteractionSlot, MAX_RECENT_INTERACTIONS>();
        }
    }

    void compressToMidTerm(uint64_t currentFrame) {
        InteractionSlot ibuf[MAX_RECENT_INTERACTIONS];
        size_t icount = interactions.getRecent(ibuf, MAX_RECENT_INTERACTIONS);
        for (size_t i = 0; i < icount; i++) {
            MidTermSummary* mids = const_cast<MidTermSummary*>(midTerm.rawData());
            size_t msize = midTerm.size();
            uint32_t target = ibuf[i].otherSlot;
            bool found = false;
            for (size_t j = 0; j < msize; j++) {
                if (mids[j].targetSlot == target && mids[j].category == 0) {
                    mids[j].interactionCount++;
                    mids[j].avgEmotionScore = static_cast<int8_t>(
                        (static_cast<int>(mids[j].avgEmotionScore) * (mids[j].interactionCount - 1) +
                         ibuf[i].impactScore) / mids[j].interactionCount);
                    if (ibuf[i].timestamp < mids[j].firstTime || mids[j].firstTime == 0)
                        mids[j].firstTime = ibuf[i].timestamp;
                    if (ibuf[i].timestamp > mids[j].lastTime)
                        mids[j].lastTime = ibuf[i].timestamp;
                    found = true;
                    break;
                }
            }
            if (!found && msize < MAX_MIDTERM) {
                MidTermSummary s;
                s.targetSlot = target;
                s.interactionCount = 1;
                s.avgEmotionScore = ibuf[i].impactScore;
                s.firstTime = ibuf[i].timestamp;
                s.lastTime = ibuf[i].timestamp;
                s.category = 0;
                midTerm.push(s);
            }
        }

        WitnessedSlot wbuf[MAX_RECENT_WITNESSED];
        size_t wcount = witnessed.getRecent(wbuf, MAX_RECENT_WITNESSED);
        for (size_t i = 0; i < wcount; i++) {
            MidTermSummary* mids = const_cast<MidTermSummary*>(midTerm.rawData());
            size_t msize = midTerm.size();
            uint32_t target = wbuf[i].slot;
            bool found = false;
            for (size_t j = 0; j < msize; j++) {
                if (mids[j].targetSlot == target && mids[j].category == 1) {
                    mids[j].interactionCount++;
                    int score = static_cast<int>(mids[j].avgEmotionScore) * (mids[j].interactionCount - 1)
                                + static_cast<int>(wbuf[i].significance);
                    mids[j].avgEmotionScore = static_cast<int8_t>(score / mids[j].interactionCount);
                    if (wbuf[i].timestamp < mids[j].firstTime || mids[j].firstTime == 0)
                        mids[j].firstTime = wbuf[i].timestamp;
                    if (wbuf[i].timestamp > mids[j].lastTime)
                        mids[j].lastTime = wbuf[i].timestamp;
                    found = true;
                    break;
                }
            }
            if (!found && msize < MAX_MIDTERM) {
                MidTermSummary s;
                s.targetSlot = target;
                s.interactionCount = 1;
                s.avgEmotionScore = static_cast<int8_t>(wbuf[i].significance);
                s.firstTime = wbuf[i].timestamp;
                s.lastTime = wbuf[i].timestamp;
                s.category = 1;
                midTerm.push(s);
            }
        }

        CommandMemorySlot cbuf[MAX_RECENT_COMMAND];
        size_t ccount = commandMemory.getRecent(cbuf, MAX_RECENT_COMMAND);
        for (size_t i = 0; i < ccount; i++) {
            MidTermSummary* mids = const_cast<MidTermSummary*>(midTerm.rawData());
            size_t msize = midTerm.size();
            uint32_t target = cbuf[i].issuerSlot;
            bool found = false;
            for (size_t j = 0; j < msize; j++) {
                if (mids[j].targetSlot == target && mids[j].category == 2) {
                    mids[j].interactionCount++;
                    int score = static_cast<int>(mids[j].avgEmotionScore) * (mids[j].interactionCount - 1)
                                + cbuf[i].influence;
                    mids[j].avgEmotionScore = static_cast<int8_t>(score / mids[j].interactionCount);
                    if (cbuf[i].timestamp < mids[j].firstTime || mids[j].firstTime == 0)
                        mids[j].firstTime = cbuf[i].timestamp;
                    if (cbuf[i].timestamp > mids[j].lastTime)
                        mids[j].lastTime = cbuf[i].timestamp;
                    found = true;
                    break;
                }
            }
            if (!found && msize < MAX_MIDTERM) {
                MidTermSummary s;
                s.targetSlot = target;
                s.interactionCount = 1;
                s.avgEmotionScore = cbuf[i].influence;
                s.firstTime = cbuf[i].timestamp;
                s.lastTime = cbuf[i].timestamp;
                s.category = 2;
                midTerm.push(s);
            }
        }
        upgradeMidTermToLongTerm(currentFrame);
    }

    int getTopMidTerm(MidTermSummary* out, int count) const {
        MidTermSummary all[MAX_MIDTERM];
        size_t n = midTerm.getRecent(all, MAX_MIDTERM);
        if (n == 0) return 0;

        struct Scored {
            MidTermSummary summary;
            int score;
        };
        Scored scored[MAX_MIDTERM];
        for (size_t i = 0; i < n; i++) {
            scored[i].summary = all[i];
            int adv = all[i].avgEmotionScore < 0 ? -all[i].avgEmotionScore : all[i].avgEmotionScore;
            scored[i].score = static_cast<int>(all[i].interactionCount) * adv;
        }
        std::sort(scored, scored + n,
            [](const Scored& a, const Scored& b) { return a.score > b.score; });

        int outCount = static_cast<int>(n) < count ? static_cast<int>(n) : count;
        for (int i = 0; i < outCount; i++) {
            out[i] = scored[i].summary;
        }
        return outCount;
    }

    int getConsecutiveFailures(uint32_t issuerSlot) const {
        CommandMemorySlot buf[MAX_RECENT_COMMAND];
        size_t n = commandMemory.getRecent(buf, MAX_RECENT_COMMAND);
        int consecutive = 0;
        for (size_t i = 0; i < n; i++) {
            if (buf[i].issuerSlot != issuerSlot) continue;
            if (buf[i].result == 2 || buf[i].result == 3) {
                consecutive++;
            } else {
                break;
            }
        }
        return consecutive;
    }

    int getOverachieveCount(uint32_t issuerSlot) const {
        CommandMemorySlot buf[MAX_RECENT_COMMAND];
        size_t n = commandMemory.getRecent(buf, MAX_RECENT_COMMAND);
        int count = 0;
        for (size_t i = 0; i < n; i++) {
            if (buf[i].issuerSlot != issuerSlot) continue;
            if (buf[i].result == 0 && buf[i].emotionTag == 1) {
                count++;
            }
        }
        return count;
    }
};
