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
#include <unordered_set>
#include <mutex>
#include <iomanip>
#include <sstream>
#include <json.hpp>
#include <functional>
#include "mqttwrapper.h"

using namespace eprosima::fastdds::dds;
using namespace eprosima::fastdds::rtps;

struct ParticipantInfo {
    std::string name;
    std::string guid;
    std::string status;
    std::vector<std::string> reading_topics;
    std::vector<std::string> writing_topics;
    std::chrono::time_point<std::chrono::system_clock> discovered_timestamp;
    unsigned int domain_id;
};

struct TopicUserSimple {
    std::string name;
    std::string guid;

    bool operator==(const TopicUserSimple& other) const {
        return (this->name == other.name) && (this->guid == other.guid);
    }
};

namespace std {
    template <>
    struct hash<TopicUserSimple> {
        std::size_t operator()(const TopicUserSimple& user) const {
            // Hash both strings
            std::size_t h1 = std::hash<std::string>{}(user.name);
            std::size_t h2 = std::hash<std::string>{}(user.guid);

            // A common way to combine two hashes:
            h1 ^= (h2 + 0x9e3779b97f4a7c15ULL + (h1 << 6) + (h1 >> 2));
            return h1;
        }
    };
}

struct TopicInfo {
    std::string name;
    std::string type;
    unsigned int num_readers;
    unsigned int num_writers;
    std::unordered_set<TopicUserSimple> readers;
    std::unordered_set<TopicUserSimple> writers;
    std::chrono::time_point<std::chrono::system_clock> discovered_timestamp;
    unsigned int domain_id;
};

std::unordered_map<std::string, ParticipantInfo> participants_cache;
std::unordered_map<std::string, TopicInfo> topics_cache;
std::mutex cache_mutex;

class CustomParticipantListener : public DomainParticipantListener {
public:

    /**
     * @brief This method is called when a participant is discovered in the DDS network.
     * 
     * @param participant The network participant.
     * @param reason The reason for the discovery.
     * @param info The participant's information (excluding the topics).
     * @param should_be_ignored Flag to ignore the participant.
     */
    void on_participant_discovery(
            DomainParticipant* participant,
            ParticipantDiscoveryStatus reason,
            const ParticipantBuiltinTopicData& info,
            bool& should_be_ignored) override 
    {

        std::lock_guard<std::mutex> lock(cache_mutex);

        auto now = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
        std::cout << "[" << now << "]" << participant_discovery_status_to_string(reason) << ": " << info.participant_name << " (" << info.guid << ")" << std::endl;
        should_be_ignored = false; // Do not ignore this reader
        std::string guid_str = guid_to_string(info.guid);
        std::string participant_name = std::string(info.participant_name.c_str());

        if (reason == ParticipantDiscoveryStatus::DISCOVERED_PARTICIPANT) {
            auto now = std::chrono::system_clock::now();
            participants_cache.emplace(guid_str, ParticipantInfo{
                participant_name,
                guid_str,
                "DISCOVERED",
                {}, // Empty reading_topics
                {}, // Empty writing_topics
                now, // Current timestamp
                participant->get_domain_id() // Domain ID
            });

        } else if (reason == ParticipantDiscoveryStatus::REMOVED_PARTICIPANT) {
            participants_cache.erase(guid_str);
        } else if (reason == ParticipantDiscoveryStatus::DROPPED_PARTICIPANT) {
            participants_cache.erase(guid_str);
        }

    }

    /**
     * @brief Convert a GUID_t to a string.
     * 
     * @param guid The GUID to convert.
     */
    std::string guid_to_string(const GUID_t& guid)
    {
        std::ostringstream oss;
        oss << guid;
        return oss.str();
    }

    /**
     * @brief Convert a ParticipantDiscoveryStatus to a string.
     * 
     * @param status The status to convert.
     */
    std::string participant_discovery_status_to_string(ParticipantDiscoveryStatus status) {
        switch (status) {
            case ParticipantDiscoveryStatus::DISCOVERED_PARTICIPANT: return "Discovered Participant";
            case ParticipantDiscoveryStatus::CHANGED_QOS_PARTICIPANT: return "Changed QoS Participant";
            case ParticipantDiscoveryStatus::REMOVED_PARTICIPANT: return "Removed Participant";
            case ParticipantDiscoveryStatus::DROPPED_PARTICIPANT: return "Dropped Participant";
            case ParticipantDiscoveryStatus::IGNORED_PARTICIPANT: return "Ignored Participant";
            default: return "Unknown Status";
        }
    }

    /**
     * @brief This method is called when a topic writer is discovered in the DDS network.
     * 
     * @param participant The network participant (topic writer).
     * @param reason The reason for the discovery.
     * @param info The writer's topic publishing information.
     * @param should_be_ignored Flag to ignore the writer.
     */
    void on_data_writer_discovery(
        DomainParticipant* participant,
        WriterDiscoveryStatus reason,
        const PublicationBuiltinTopicData& info,
        bool& should_be_ignored) override
    {
        std::lock_guard<std::mutex> lock(cache_mutex);

        auto now = std::chrono::system_clock::now();
        std::cout << "[" << std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch()).count() << "]" << writer_discovery_status_to_string(reason) << ": " << info.topic_name << " [" << info.type_name << "] (" << info.participant_guid << ")" << std::endl;
        std::string guid_str = guid_to_string(info.participant_guid);
        std::string participant_name = participants_cache[guid_str].name;
        std::string topic_name = std::string(info.topic_name.c_str());
        std::string type_name = std::string(info.type_name.c_str());

        // Update topics_cache
        auto [it, inserted] = topics_cache.emplace(topic_name, TopicInfo{topic_name, type_name, 0, 0, {}, {}, {}, participant->get_domain_id()});
        auto& topic_info = it->second;

        if (reason == WriterDiscoveryStatus::DISCOVERED_WRITER) {
            TopicUserSimple writer = {participant_name, guid_str};
            topic_info.writers.insert(writer);
            topic_info.num_writers++;

            // Set the timestamp only if this is the first writer for the topic
            if (topic_info.discovered_timestamp.time_since_epoch().count() == 0) {
                topic_info.discovered_timestamp = now;
            }

            // Update the participant's writing topics
            auto participant_it = participants_cache.find(guid_str);
            if (participant_it != participants_cache.end()) {
                auto& participant = participant_it->second;
                if (std::find(participant.writing_topics.begin(), participant.writing_topics.end(), topic_name) == participant.writing_topics.end()) {
                    participant.writing_topics.push_back(topic_name);
                }
            }

        } else if (reason == WriterDiscoveryStatus::REMOVED_WRITER) {
            TopicUserSimple writer = {participant_name, guid_str};
            topic_info.writers.erase(writer);
            topic_info.num_writers--;

            // Remove the topic from the participant's writing_topics
            auto participant_it = participants_cache.find(guid_str);
            if (participant_it != participants_cache.end()) {
                auto& participant = participant_it->second;
                participant.writing_topics.erase(std::remove(participant.writing_topics.begin(), participant.writing_topics.end(), topic_name), participant.writing_topics.end());
            }

            // If there are no more writers and readers for this topic, remove it from the cache
            if (topic_info.writers.empty() && topic_info.readers.empty()) {
                topics_cache.erase(topic_name);
            }
        }
        should_be_ignored = false;
    }

    /**
     * @brief Convert a WriterDiscoveryStatus to a string.
     * 
     * @param status The status to convert.
     */
    std::string writer_discovery_status_to_string(WriterDiscoveryStatus status) {
        switch (status) {
            case WriterDiscoveryStatus::DISCOVERED_WRITER: return "Discovered Writer";
            case WriterDiscoveryStatus::CHANGED_QOS_WRITER: return "Changed QoS Writer";
            case WriterDiscoveryStatus::REMOVED_WRITER: return "Removed Writer";
            case WriterDiscoveryStatus::IGNORED_WRITER: return "Ignored Writer";
            default: return "Unknown Status";
        }
    }

    /**
     * @brief This method is called when a topic reader is discovered in the DDS network.
     * 
     * @param participant The network participant (topic reader).
     * @param reason The reason for the discovery.
     * @param info The reader's topic subscription information.
     * @param should_be_ignored Flag to ignore the reader.
     */
    void on_data_reader_discovery(
        DomainParticipant* participant,
        ReaderDiscoveryStatus reason,
        const SubscriptionBuiltinTopicData& info,
        bool& should_be_ignored) override
    {
        std::lock_guard<std::mutex> lock(cache_mutex);

        auto now = std::chrono::system_clock::now();
        std::cout << "[" << std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch()).count() << "]" << reader_discovery_status_to_string(reason) << ": " << info.topic_name << " [" << info.type_name << "] (" << info.participant_guid << ")" << std::endl;
        std::string guid_str = guid_to_string(info.participant_guid);
        std::string participant_name = participants_cache[guid_str].name;
        std::string topic_name = std::string(info.topic_name.c_str());
        std::string type_name = std::string(info.type_name.c_str());

        // Update topics_cache
        auto [it, inserted] = topics_cache.emplace(topic_name, TopicInfo{topic_name, type_name, 0, 0, {}, {}, {}, participant->get_domain_id()});
        auto& topic_info = it->second;

        if (reason == ReaderDiscoveryStatus::DISCOVERED_READER) {
            TopicUserSimple reader = {participant_name, guid_str};
            topic_info.readers.insert(reader);
            topic_info.num_readers++;

            // Set the timestamp only if this is the first reader for the topic
            if (topic_info.discovered_timestamp.time_since_epoch().count() == 0) {
                topic_info.discovered_timestamp = now;
            }

            // Update the participant's reading topics
            auto participant_it = participants_cache.find(guid_str);
            if (participant_it != participants_cache.end()) {
                auto& participant = participant_it->second;
                if (std::find(participant.reading_topics.begin(), participant.reading_topics.end(), topic_name) == participant.reading_topics.end()) {
                    participant.reading_topics.push_back(topic_name);
                }
            }
        } else if (reason == ReaderDiscoveryStatus::REMOVED_READER) {
            TopicUserSimple reader = {participant_name, guid_str};
            topic_info.readers.erase(reader);
            topic_info.num_readers--;

            // Remove the topic from the participant's reading_topics
            auto participant_it = participants_cache.find(guid_str);
            if (participant_it != participants_cache.end()) {
                auto& participant = participant_it->second;
                participant.reading_topics.erase(std::remove(participant.reading_topics.begin(), participant.reading_topics.end(), topic_name), participant.reading_topics.end());
            }

            // If there are no more writers and readers for this topic, remove it from the cache
            if (topic_info.writers.empty() && topic_info.readers.empty()) {
                topics_cache.erase(topic_name);
            }
        }
        should_be_ignored = false;
    }

    /**
     * @brief Convert a ReaderDiscoveryStatus to a string.
     * 
     * @param status The status to convert.
     */
    std::string reader_discovery_status_to_string(ReaderDiscoveryStatus status) {
        switch (status) {
            case ReaderDiscoveryStatus::DISCOVERED_READER: return "Discovered Reader";
            case ReaderDiscoveryStatus::CHANGED_QOS_READER: return "Changed QoS Reader";
            case ReaderDiscoveryStatus::REMOVED_READER: return "Removed Reader";
            case ReaderDiscoveryStatus::IGNORED_READER: return "Ignored Reader";
            default: return "Unknown Status";
        }
    }
};

/**
 * @brief Convert a time_point to a string.
 * 
 * @param tp The time_point.
 */
std::string time_point_to_string(const std::chrono::time_point<std::chrono::system_clock>& tp) {
    auto duration = tp.time_since_epoch();
    auto seconds = std::chrono::duration_cast<std::chrono::seconds>(duration).count();
    auto microseconds = std::chrono::duration_cast<std::chrono::microseconds>(duration).count() % 1000000;

    std::ostringstream oss;
    oss << seconds << "." << std::setfill('0') << std::setw(6) << microseconds;
    return oss.str();
}

/**
 * @brief Convert a time_point to a human-readable string.
 * 
 * @param tp The time_point.
 */
std::string time_point_to_human_string(const std::chrono::time_point<std::chrono::system_clock>& tp) {
    std::time_t t = std::chrono::system_clock::to_time_t(tp);
    std::tm tm = *std::localtime(&t);
    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S");
    return oss.str();
}

/**
 * @brief Serialize ParticipantInfo to JSON.
 * 
 * @param participant The participant information to serialize.
 */
std::string serialize_participant(const ParticipantInfo& participant) {
    nlohmann::json json_obj = {
        {"name", participant.name},
        {"guid", participant.guid},
        {"status", participant.status},
        {"reading_topics", participant.reading_topics},
        {"writing_topics", participant.writing_topics},
        {"discovered_timestamp", time_point_to_string(participant.discovered_timestamp)},
        {"discovered_timestamp_simple", time_point_to_human_string(participant.discovered_timestamp)},
        {"domain_id", participant.domain_id}
    };
    return json_obj.dump(4); // 4 spaces indentation
}

/**
 * @brief Serialize a TopicUserSimple to JSON.
 * 
 * @param user The user information to serialize.
 */

std::string serialize_user_simple(const TopicUserSimple& user) {
    nlohmann::json json_obj = {
        {"name", user.name},
        {"guid", user.guid}
    };
    return json_obj.dump(4); // 4 spaces indentation
}

/**
 * @brief Serialize a set of TopicUserSimple to JSON.
 * 
 * @param users The set of users to serialize.
 */
std::string serialize_user_simple_set(const std::unordered_set<TopicUserSimple>& users) {
    nlohmann::json json_array = nlohmann::json::array();
    for (const auto& user : users)
    {
        json_array.push_back(nlohmann::json::parse(serialize_user_simple(user)));
    }
    return json_array.dump(4); // 4 spaces indentation
}

/**
 * @brief Serialize TopicInfo to JSON.
 * 
 * @param topic The topic information to serialize.
 */
std::string serialize_topic(const TopicInfo& topic) {
    nlohmann::json json_obj = {
        {"name", topic.name},
        {"type", topic.type},
        {"num_readers", topic.num_readers},
        {"num_writers", topic.num_writers},
        {"readers", nlohmann::json::parse(serialize_user_simple_set(topic.readers))},
        {"writers", nlohmann::json::parse(serialize_user_simple_set(topic.writers))},
        {"discovered_timestamp", time_point_to_string(topic.discovered_timestamp)},
        {"discovered_timestamp_simple", time_point_to_human_string(topic.discovered_timestamp)},
        {"domain_id", topic.domain_id}
    };
    return json_obj.dump(4); // 4 spaces indentation
}

/**
 * @brief Serialize a map of participants.
 * 
 * @param participants_cache The participants cache that contains all the participants information.
 */
std::string serialize_participants(const std::unordered_map<std::string, ParticipantInfo>& participants_cache) {
    nlohmann::json json_array = nlohmann::json::array();
    for (const auto& [guid, participant] : participants_cache)
    {
        json_array.push_back({
            {"guid", guid},
            {"info", nlohmann::json::parse(serialize_participant(participant))}
        });
    }
    return json_array.dump(4); // 4 spaces indentation
}

/**
 * @brief Serialize a map of topics.
 * 
 * @param topics_cache The topics cache that contains all the topics information.
 */
std::string serialize_topics(const std::unordered_map<std::string, TopicInfo>& topics_cache) {
    nlohmann::json json_array = nlohmann::json::array();
    for (const auto& [name, topic] : topics_cache)
    {
        json_array.push_back({
            {"name", name},
            {"info", nlohmann::json::parse(serialize_topic(topic))}
        });
    }
    return json_array.dump(4); // 4 spaces indentation
}

std::string empty (std::string a, std::string b) {
    return "";
}

int main( int argc, char* argv[] ) {
    int domain_id = 0; // DDS domain ID
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

    if (participant == nullptr) {
        std::cerr << "Failed to create DomainParticipant." << std::endl;
        return EXIT_FAILURE;
    }

    // Attach a custom listener
    CustomParticipantListener listener;
    participant->set_listener(&listener);

    std::cout << "Monitoring DDS topics in domain " << domain_id << "..." << std::endl;

    // MQTT setup
    bool mqtt_participant_connected = false;
    bool mqtt_topic_connected = false;
    std::string mqtt_host;
    if (argc > 2) mqtt_host = argv[2];
    else mqtt_host = "127.0.0.1";

    std::cout << "Setting up MQTT on " << mqtt_host << "..." << std::endl;

    data_mqtt_server mqtt_participants;
    mqtt_participants.client_id = "dds-monitor-participants";
    mqtt_participants.address = "tcp://" + mqtt_host + ":1883";
    mqtt_participants.publish_topic = "dds/participants";

    data_mqtt_server mqtt_topics;
    mqtt_topics.client_id = "dds-monitor-topics";
    mqtt_topics.address = "tcp://" + mqtt_host + ":1883";
    mqtt_topics.publish_topic = "dds/topics";

    MqttWrapper * mqtt_participants_wrapper = new MqttWrapper(mqtt_participants, empty);
    MqttWrapper * mqtt_topics_wrapper = new MqttWrapper(mqtt_topics, empty);

    int max_retries = 10;
    int retries = 0;
    while (!mqtt_participants_wrapper->is_connected() && retries < max_retries) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        retries++;
    }
    if (retries == max_retries) std::cerr << "Failed to connect to MQTT broker." << std::endl;
    else {
        std::cout << "Connected to MQTT broker." << std::endl;
        mqtt_participant_connected = true;
    }

    retries = 0;
    while (!mqtt_topics_wrapper->is_connected() && retries < max_retries) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        retries++;
    }

    if (retries == max_retries) std::cerr << "Failed to connect to MQTT broker." << std::endl;
    else {
        std::cout << "Connected to MQTT broker." << std::endl;
        mqtt_topic_connected = true;
    }

    // Keep the program running to monitor events
    while (true) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        {
            std::lock_guard<std::mutex> lock(cache_mutex); // Ensure thread safety
            std::cout << "----------------------------------------\n" << std::endl;
            std::string participants_json = serialize_participants(participants_cache);
            std::string topics_json = serialize_topics(topics_cache);
            std::cout << "Participants:\n" << participants_json << "\n";
            std::cout << "Topics:\n" << topics_json << "\n";

            if (mqtt_participant_connected) mqtt_participants_wrapper->publish("dds/participants", participants_json);
            if (mqtt_topic_connected) mqtt_topics_wrapper->publish("dds/topics", topics_json);
        }
    }

    // Clean up
    participant->set_listener(nullptr);
    DomainParticipantFactory::get_instance()->delete_participant(participant);

    return EXIT_SUCCESS;
}
