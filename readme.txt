client.cpp takes authentication inputs and course query inputs and sends them to serverM. Client also receives the authentication result and course query result from serverM. 
serverM.cpp takes authentication inputs, encrypts them, sends them to serverC, receives the authentication result from serverC, and sends it back to client. serverM also takes course query inputs, sends them to serverCS/EE, receives the course query results and sends the results back to client.
serverC.cpp takes encrypted authentication request, parses the cred.txt file, verifies the authentication request, and sends the result to serverM.
serverCS.cpp takes course query request, parses the cs.txt file, compares the course query request against the file, and sends the result to serverM.
serverEE.cpp takes course query request, parses the ee.txt file, compares the course query request against the file, and sends the result to serverM.
For authentication request/reply among client, serverM, and serverC, I used integer encoding; 0 means Username failure, 1 means Password failure, and 2 means Authentication success. 
For course query request from client to serverM and from serverM to serverCS/EE, I sent the query directly as character array; For example, "EE450" for course code and "Credit" for category. 
For course query reply from serverCS/EE to serverM and from serverM to client, I again sent the query result directly as character array for success. When the course was not found, I sent "COURSE NOT FOUND". 
