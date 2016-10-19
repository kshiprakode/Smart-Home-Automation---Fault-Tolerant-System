Assumptions -

The system works in the following way :

1. After the door is closed, the events are caught and the system response are -

Motion		Keychain	Security-System response	Reason
False		False		ON				User is not at home so switch the security system, on.
True		True		OFF				User is back home so switch the system ,off
True		False		Intruder alarm			There is a presence of an intruder
False		True		OFF				User is already at home, so no need to ON the security system

For other cases no change is done to the security system and gateway does not display anything.


2. The gateway interface will show the process for 2PC.

3. The Motion, Door, Keychain Sensors and Smart Device output files and interface show only the messages it sends. the vector for that is processed and updated and then it sends the message.

6. Vector Structure is [Motion,Keychain, Door, Gateway]

7. Gateway take time to get the data from the sensors and analyse the result and print it.


