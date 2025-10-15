namespace TokenProvisioningService;

public class TokenProvisioningOptions
{
    public string JwtSecretKey { get; set; } = string.Empty;
    public string JwtIssuer { get; set; } = "TokenProvisioningService";
    public string JwtAudience { get; set; } = "CYD-MQTT-Devices";
    public int TokenExpirationMinutes { get; set; } = 60;
    public int UdpPort { get; set; } = 12345;
    public string DatabasePath { get; set; } = "devices.db";
}