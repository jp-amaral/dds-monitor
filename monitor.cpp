#include <fastdds/dds/domain/DomainParticipantFactory.hpp>
#include <fastdds/dds/domain/DomainParticipant.hpp>
#include <fastdds/dds/domain/DomainParticipantListener.hpp>
#include <fastdds/dds/builtin/topic/PublicationBuiltinTopicData.hpp>
#include <fastdds/dds/builtin/topic/SubscriptionBuiltinTopicData.hpp>
#include <fastdds/rtps/participant/ParticipantDiscoveryInfo.hpp>
#include <fastdds/rtps/reader/ReaderDiscoveryStatus.hpp>
#include <fastdds/rtps/writer/WriterDiscoveryStatus.hpp>

#include <iostream>
#include <thread>
#include <chrono>
#include <unordered_map>

using namespace eprosima::fastdds::dds;
using namespace eprosima::fastdds::rtps;

struct ParticipantInfo {
    std::string name;
    std::string guid;
    std::string status;
    std::vector<std::string> associated_topics;
};

struct TopicInfo {
    std::string name;
    std::string type;
    std::vector<std::string> readers;
    std::vector<std::string> writers;
};

class CustomParticipantListener : public DomainParticipantListener
{
public:
    void on_participant_discovery(
            DomainParticipant* participant,
            ParticipantDiscoveryStatus reason,
            const ParticipantBuiltinTopicData& info,
            bool& should_be_ignored) override
    {
        auto now = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
        std::cout << "[" << now << "]" << participant_discovery_status_to_string(reason) << ": " << info.participant_name << " (" << info.guid << ")" << std::endl;
        should_be_ignored = false; // Do not ignore this reader
    }

    std::string participant_discovery_status_to_string(ParticipantDiscoveryStatus status)
    {
        switch (status)
        {
            case ParticipantDiscoveryStatus::DISCOVERED_PARTICIPANT: return "Discovered Participant";
            case ParticipantDiscoveryStatus::CHANGED_QOS_PARTICIPANT: return "Changed QoS Participant";
            case ParticipantDiscoveryStatus::REMOVED_PARTICIPANT: return "Removed Participant";
            case ParticipantDiscoveryStatus::DROPPED_PARTICIPANT: return "Dropped Participant";
            case ParticipantDiscoveryStatus::IGNORED_PARTICIPANT: return "Ignored Participant";
            default: return "Unknown Status";
        }
    }

    void on_data_writer_discovery(
        DomainParticipant* participant,
        WriterDiscoveryStatus reason,
        const PublicationBuiltinTopicData& info,
        bool& should_be_ignored) override
    {
        auto now = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
        std::cout << "[" << now << "]" << writer_discovery_status_to_string(reason) << ": " << info.topic_name << " [" << info.type_name << "] (" << info.participant_guid << ")" << std::endl;
        should_be_ignored = false; // Do not ignore this writer
    }

    std::string writer_discovery_status_to_string(WriterDiscoveryStatus status)
    {
        switch (status)
        {
            case WriterDiscoveryStatus::DISCOVERED_WRITER: return "Discovered Writer";
            case WriterDiscoveryStatus::CHANGED_QOS_WRITER: return "Changed QoS Writer";
            case WriterDiscoveryStatus::REMOVED_WRITER: return "Removed Writer";
            case WriterDiscoveryStatus::IGNORED_WRITER: return "Ignored Writer";
            default: return "Unknown Status";
        }
    }

    void on_data_reader_discovery(
        DomainParticipant* participant,
        ReaderDiscoveryStatus reason,
        const SubscriptionBuiltinTopicData& info,
        bool& should_be_ignored) override
    {
        auto now = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
        std::cout << "[" << now << "]" << reader_discovery_status_to_string(reason) << ": " << info.topic_name << " [" << info.type_name << "] (" << info.participant_guid << ")" << std::endl;
        should_be_ignored = false; // Do not ignore this reader
    }

    std::string reader_discovery_status_to_string(ReaderDiscoveryStatus status)
    {
        switch (status)
        {
            case ReaderDiscoveryStatus::DISCOVERED_READER: return "Discovered Reader";
            case ReaderDiscoveryStatus::CHANGED_QOS_READER: return "Changed QoS Reader";
            case ReaderDiscoveryStatus::REMOVED_READER: return "Removed Reader";
            case ReaderDiscoveryStatus::IGNORED_READER: return "Ignored Reader";
            default: return "Unknown Status";
        }
    }
};

int main( int argc, char* argv[] )
{
    int domain_id = 0; // Specify your DDS domain ID
    if (argc > 1) {
        try {
            domain_id = std::stoi(argv[1]);
        } catch (const std::exception& e) {
            std::cerr << "Invalid domain ID: " << argv[1] << std::endl;
            return EXIT_FAILURE;
        }
    }
    if (domain_id < 0 || domain_id > 230) {
        std::cerr << "Domain ID out of range: " << domain_id << std::endl;
        return EXIT_FAILURE;
    }

    // Create DomainParticipant
    DomainParticipantQos participant_qos = PARTICIPANT_QOS_DEFAULT;
    DomainParticipant* participant = DomainParticipantFactory::get_instance()->create_participant(domain_id, participant_qos);

    if (participant == nullptr)
    {
        std::cerr << "Failed to create DomainParticipant." << std::endl;
        return EXIT_FAILURE;
    }

    // Attach a custom listener
    CustomParticipantListener listener;
    participant->set_listener(&listener);

    std::cout << "Monitoring DDS topics in domain " << domain_id << "..." << std::endl;

    // Keep the program running to monitor events
    while (true)
    {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    // Clean up
    participant->set_listener(nullptr);
    DomainParticipantFactory::get_instance()->delete_participant(participant);

    return EXIT_SUCCESS;
}
