<Query Kind="Statements">
  <NuGetReference>MQTTnet</NuGetReference>
  <Namespace>MQTTnet</Namespace>
  <Namespace>System.Threading.Tasks</Namespace>
</Query>

var mqttFactory = new MqttClientFactory();
var rnd = new Random();

using var mqttClient = mqttFactory.CreateMqttClient();
var mqttClientOptions = new MqttClientOptionsBuilder()
								.WithTcpServer("enterprise2u.local", 1883)
								.WithClientId("Linqpad")
								.WithCredentials(Util.GetPassword("picollect.nats.username"), Util.GetPassword("picollect.nats.password"))
								.WithProtocolVersion(MQTTnet.Formatter.MqttProtocolVersion.V311)
								.Build();

await mqttClient.ConnectAsync(mqttClientOptions, CancellationToken.None);
Console.WriteLine("Connected...");

for (var i = 0; i < 100; ++i)
{
	var temp = rnd.Next(-100, 100);
	var applicationMessage = new MqttApplicationMessageBuilder()
		.WithTopic("home/livingroom/temperature")
		.WithPayload(temp.ToString())
		.Build();

	await mqttClient.PublishAsync(applicationMessage, CancellationToken.None);
	Console.WriteLine($"Published {i}... {temp}");

	await Task.Delay(TimeSpan.FromMilliseconds(rnd.Next(2, 1000)));
}

var easterEggMessage = new MqttApplicationMessageBuilder()
	.WithTopic("home/livingroom/temperature")
	.WithPayload("88.7")
	.Build();

await mqttClient.PublishAsync(easterEggMessage, CancellationToken.None);

await mqttClient.DisconnectAsync();

Console.WriteLine("MQTT application messages published.");
