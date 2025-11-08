// OverteNetworkClient.hpp
#pragma once

#ifdef HAVE_OVERTE_NETWORKING

#include <memory>
#include <functional>
#include <string>
#include <QObject>
#include <QSharedPointer>

// Forward declarations to avoid including full Overte headers here
class DomainHandler;
class NodeList;
class Node;
class NLPacket;

namespace OverteNetworking {

class NetworkClient : public QObject {
    Q_OBJECT
    
public:
    NetworkClient();
    ~NetworkClient();
    
    // Connect to domain server
    bool connectToDomain(const QString& domainURL, 
                        const QString& username = QString(), 
                        const QString& password = QString());
    
    // Disconnect from domain
    void disconnect();
    
    // Check if connected
    bool isConnected() const;
    
    // Process network events (call from main loop)
    void processEvents();
    
    // Callback for when entities are received
    using EntityCallback = std::function<void(const QByteArray& entityData)>;
    void setEntityCallback(EntityCallback callback);
    
private slots:
    void onDomainChanged(const QUrl& domainURL);
    void onDomainConnectionRefused(const QString& reasonMessage, int reasonCode);
    void onConnectedToDomain(const QString& hostname);
    void onNodeAdded(SharedNodePointer node);
    void onNodeActivated(SharedNodePointer node);
    
private:
    void setupDomainHandler();
    void setupNodeList();
    void handleEntityServerPacket(QSharedPointer<ReceivedMessage> message);
    
    std::unique_ptr<DomainHandler> m_domainHandler;
    QSharedPointer<NodeList> m_nodeList;
    EntityCallback m_entityCallback;
    bool m_connected{false};
};

} // namespace OverteNetworking

#endif // HAVE_OVERTE_NETWORKING
