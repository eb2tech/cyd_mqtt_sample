<Query Kind="Statements">
  <NuGetReference>MQTTnet</NuGetReference>
  <NuGetReference>MQTTnet.Extensions.TopicTemplate</NuGetReference>
  <Namespace>MQTTnet</Namespace>
  <Namespace>MQTTnet.Extensions.TopicTemplate</Namespace>
  <Namespace>System.Threading.Tasks</Namespace>
</Query>

var mqttFactory = new MqttClientFactory();

using var mqttClient = mqttFactory.CreateMqttClient();
var mqttClientOptions = new MqttClientOptionsBuilder()
								.WithTcpServer("pikiosk.local")
								.WithClientId("Linqpad")
								.Build();

mqttClient.ApplicationMessageReceivedAsync += e =>
{
	e.ApplicationMessage.Topic.Dump();
	return Task.CompletedTask;
};

await mqttClient.ConnectAsync(mqttClientOptions, QueryCancelToken);
Console.WriteLine("Connected...");

var mqttSubscribeOptions = mqttFactory.CreateSubscribeOptionsBuilder()
									  .WithTopicFilter("homeassistant/#")
									  .Build();
									  
await mqttClient.SubscribeAsync(mqttSubscribeOptions, QueryCancelToken);
Console.WriteLine("Subscribed");

try
{
	await Task.Delay(Timeout.Infinite, QueryCancelToken);
}
catch (TaskCanceledException)
{}
finally
{
	Console.WriteLine("Done");
}