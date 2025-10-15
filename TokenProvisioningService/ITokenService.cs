namespace TokenProvisioningService;

public interface ITokenService
{
    Task<string> GenerateTokenAsync(string deviceId, string uuid);
    Task<bool> ValidateDeviceAsync(string uuid);
}