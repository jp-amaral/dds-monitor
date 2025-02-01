#ifndef DCC_FACILITIES_MQTTWRAPPER_H
#define DCC_FACILITIES_MQTTWRAPPER_H

#include <string>
#include <thread>
#include <mqtt/async_client.h>
#include <mqtt/callback.h>
#include <mqtt/iaction_listener.h>
#include "mqtt/async_client.h"

/** @struct data_mqtt_server
 *  @brief Represents the MQTT server data.
 */
struct data_mqtt_server {
    std::string address;         /**< Server address: IP + Port */
    std::string client_id;       /**< Client ID for MQTT connection (must be unique) */
    std::vector<std::string> subscription_topic; /**< List of topics to subscribe to */
    std::string publish_topic;   /**< Topic for publishing messages */
};

/** @class action_listener
 *  @brief A listener for MQTT action events.
 */
class action_listener : public virtual mqtt::iaction_listener {
    std::string name_; /**< Name associated with the listener. */

public:
    /** @brief Constructor.
     *  @param name The name associated with the listener.
     */
    explicit action_listener(std::string name);

    /** @brief Callback on action failure.
     *  @param tok The token associated with the action.
     */
    void on_failure(const mqtt::token& tok) override;

    /** @brief Callback on action success.
     *  @param tok The token associated with the action.
     */
    void on_success(const mqtt::token& tok) override;

    /** @brief Virtual destructor.
     */
    virtual ~action_listener();  // Add the virtual destructor
};

/** @class MqttWrapper
 *  @brief Wrapper class for handling MQTT operations.
 */
class MqttWrapper : public virtual mqtt::callback, public virtual mqtt::iaction_listener {
public:
    /** @brief Constructor Publish and Subscribe
     *  @param data MQTT server data. Must contain the server address (IP + Port), the client ID (unique) and the list of topics to subscribe (It can be an empty list if you don't want to subscribe to any topic)
     *  @param on_message_received Callback function for handling received messages.
     */
    MqttWrapper(data_mqtt_server data, std::function<void(std::string, std::string)> on_message_received);

    /** @brief Constructor Publish
     *  @param data MQTT server data. Must contain the server address (IP + Port), the client ID (unique) and the list of topics to subscribe (It can be an empty list if you don't want to subscribe to any topic)
     */
    MqttWrapper(data_mqtt_server data);

    /** @brief Virtual destructor.
     */
    ~MqttWrapper();

    /** @brief Publishes a message to a specific MQTT topic.
     *  @param topic The topic to publish to.
     *  @param message The message to publish.
     */
    void publish(const std::string &topic, const std::string &message);

    /** @brief Disconnect from the MQTT server.
     */
    void disconnect();

    /** @brief Determines if this client is currently connected to the server.
     *  @return true if connected, false otherwise.
     */
    bool is_connected ();

    mqtt::async_client client_; /**< MQTT async client instance. */


private:
    /** @brief Establishes connection to the MQTT server.
     */
    void connect();

    /** @brief Callback on successful MQTT operation.
     *  @param tok The token associated with the operation.
     */
    void on_success(const mqtt::token& tok) override;

    /** @brief Callback on failed MQTT operation.
     *  @param tok The token associated with the operation.
     */
    void on_failure(const mqtt::token& tok) override ;

    /** @brief Callback on successful connection to the MQTT server.
     *  @param cause The cause of the connection.
     */
    void connected(const std::string& cause) override ;

    /** @brief Reconnects to the MQTT server.
     */
    void reconnect() ;

    /** @brief Callback on lost connection to the MQTT server.
     *  @param cause The cause of the disconnection.
     */
    void connection_lost(const std::string& cause) override ;

    /** @brief Subscribes to a specific MQTT topic.
     *  @param topic The topic to subscribe to.
     */
    void subscribe(const std::string topic);

    /** @brief Callback on receiving an MQTT message.
     *  @param msg The received MQTT message.
     */
    void message_arrived(mqtt::const_message_ptr msg) override ;

    /** @brief Callback on completing the delivery of an MQTT message.
     *  @param token The token associated with the delivered message.
     */
    void delivery_complete(mqtt::delivery_token_ptr token) override;

    /** @brief Creates a forever loop.
    */
    void mqtt_thread();


    action_listener listener_; /**< Action listener instance. */
    mqtt::connect_options connOpts_; /**< MQTT connection options. */
    std::function<void(std::string, std::string)> on_message_function; /**< Callback function for received messages. */
    data_mqtt_server data_server; /**< MQTT server data. */

};



#endif //DCC_FACILITIES_MQTTWRAPPER_H
