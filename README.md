# gdbus-sdbus-test

Simple test cases to test the interaction between gdbus and sd-bus.

The Makefile is pretty bare-bones and doesn't actually check that any
dependencies are installed.

All clients/servers use the same dbus protocol.

The object manager lives at org.gnome.TestCase and exports objects under
/org/gnome/TestCase with interface org.gnome.TestCase.TestDevice.

gdbus-client: hangs around, notes server coming and going and objects
appearing/disappearing

sdbus-client: dumps all objects and quits immediately

gdbus-server: creates an objectmanager and cycles through adding objects (max
2) and deleting objects every 10 seconds.

sdbus-server: ditto.

