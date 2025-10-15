namespace TokenProvisioningService;

public class MqttBrokerOptions
{
    public string Host { get; set; } = "localhost";
    public int Port { get; set; } = 1883;
    public int SecurePort { get; set; } = 8883;
    public bool UseTls { get; set; } = false;
}