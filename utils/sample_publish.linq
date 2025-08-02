<Query Kind="Statements">
  <NuGetReference>MQTTnet</NuGetReference>
  <Namespace>MQTTnet</Namespace>
</Query>

var mqttFactory = new MqttClientFactory();

using (var mqttClient = mqttFactory.CreateMqttClient())
{
	var mqttClientOptions = new MqttClientOptionsBuilder()
		.WithTcpServer("192.168.1.92")
		.WithClientId("Linqpad")
		.WithCredentials("cyd", "cyd")
		.Build();

	await mqttClient.ConnectAsync(mqttClientOptions, CancellationToken.None);

	var applicationMessage = new MqttApplicationMessageBuilder()
		.WithTopic("home/livingroom/temperature")
		.WithPayload("88.7")
		.Build();

	await mqttClient.PublishAsync(applicationMessage, CancellationToken.None);

	await mqttClient.DisconnectAsync();

	Console.WriteLine("MQTT application message is published.");
}
