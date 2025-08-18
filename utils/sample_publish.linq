<Query Kind="Statements">
  <NuGetReference>MQTTnet</NuGetReference>
  <Namespace>MQTTnet</Namespace>
  <Namespace>System.Threading.Tasks</Namespace>
</Query>

var mqttFactory = new MqttClientFactory();
var rnd = new Random();

using var mqttClient = mqttFactory.CreateMqttClient();
var mqttClientOptions = new MqttClientOptionsBuilder()
								.WithTcpServer("homeassistant2.local")
								.WithClientId("Linqpad")
								.WithCredentials("cyd", "cyd")
								.Build();

await mqttClient.ConnectAsync(mqttClientOptions, CancellationToken.None);
Console.WriteLine("Connected...");

for (var i = 0; i < 100; ++i)
{
	var applicationMessage = new MqttApplicationMessageBuilder()
		.WithTopic("home/livingroom/temperature")
		.WithPayload(rnd.Next(-100, 100).ToString())
		.Build();
	
	await mqttClient.PublishAsync(applicationMessage, CancellationToken.None);
	Console.WriteLine("Published...");

	await Task.Delay(TimeSpan.FromSeconds(1));
}

var easterEggMessage = new MqttApplicationMessageBuilder()
	.WithTopic("home/livingroom/temperature")
	.WithPayload("88.7")
	.Build();

await mqttClient.PublishAsync(easterEggMessage, CancellationToken.None);

await mqttClient.DisconnectAsync();

Console.WriteLine("MQTT application messages published.");
