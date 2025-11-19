# Dynamic Load Balancer with Real-Time Health Monitoring
A high-availability load-balancing system written in C++, designed to efficiently manage client-server communication by routing clients to available servers, performing continuous health checks, and automatically removing or restoring servers based on availability.

## Key features
- Intelligent Routing: Connects clients to the first available free server for optimal performance.
- Real-Time Health Monitoring: Continuously pings servers and removes unresponsive nodes automatically.
- Fault Tolerance: Auto-restores servers to the pool once they come back online.
- Scalable Design: Supports multiple clients and servers simultaneously.
- Full-Duplex Messaging: Enables real-time chat between connected clients and servers.
- Efficient Resource Utilization: Ensures balanced workload distribution across servers.

## How it works
1. Load Balancer starts and listens for client connections.
2. Servers register themselves, and the load balancer tracks their status.
3. Health check threads continuously ping all servers.
4. If a server fails, it is removed from the available pool.
5. When a server recovers, it is automatically re-added.
6. Clients connect and are assigned to the first available free server.
7. Client and server establish real-time chat communication.

## Architecture Overview
          +-------------------+
          |   Load Balancer   |
          |-------------------|
   Client | Routing + Health  |  Server Pool
 <------> | Checks + Recovery | <---------->
          +-------------------+

## Build and run
```
# Compile
g++ load_balancer.cpp -o load_balancer -pthread
g++ server.cpp -o server -pthread
g++ client.cpp -o client -pthread

# Start system
./load_balancer
./server
./client
```

## Future Improvements
- Web dashboard for server monitoring
- Encrypted communication via TLS
- Containerization using Docker
