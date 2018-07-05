# NoiseGenerator
A set of two applications - client and server communicating using TCP and UDP (optionally) protocols. Client app should be able to generate noise (zeroed payload) traffic with specified bitrate evenly distributed in time (no bursts) for specified duration. In case of congested or insufficient performance network the application should lower the bitrate until it is able to push the data without queueing. Server application should display all incoming connection stats including realtime bitrate coming from the client.
