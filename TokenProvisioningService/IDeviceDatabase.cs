namespace TokenProvisioningService;

public interface IDeviceDatabase
{
    Task<bool> IsDeviceRegisteredAsync(string uuid);
    Task RegisterDeviceAsync(string uuid, string deviceId);
    Task LogTokenIssuanceAsync(string deviceId, string tokenId, DateTime expiresAt);
}