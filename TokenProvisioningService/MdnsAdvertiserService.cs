using Makaretu.Dns;
using Microsoft.Extensions.Options;

namespace TokenProvisioningService;

public class MdnsAdvertiserService : BackgroundService
{
    private readonly ILogger<MdnsAdvertiserService>? _logger;
    private readonly ushort _advertisedPort;
    private readonly string _version;
    private readonly string _uuid;

    public MdnsAdvertiserService(ILogger<MdnsAdvertiserService> logger, IOptions<TokenProvisioningOptions> provisioningOptions)
    {
        _logger = logger;
        _advertisedPort = provisioningOptions.Value.Port;
        _version = "1.0";
        _uuid = Guid.NewGuid().ToString("D");
    }

    protected override async Task ExecuteAsync(CancellationToken stoppingToken)
    {
        _logger?.LogInformation("Registering mDNS service \"CYD Provisioning\" on port {Port}", _advertisedPort);

        var mdns = new MulticastService();
        var provisioningService = new ServiceProfile("CYD Provisioning", "_cyd-provision._tcp", _advertisedPort, sharedProfile:true);
        provisioningService.AddProperty("version", _version);
        provisioningService.AddProperty("uuid", _uuid);

        var sd = new ServiceDiscovery(mdns);
        mdns.Start();
        
        try
        {
            // Register the service and keep it registered until cancellation is requested.
            sd.Advertise(provisioningService);
            sd.Announce(provisioningService);
            _logger?.LogInformation("mDNS service registered. Waiting for cancellation to unregister.");

            try
            {
                // Wait indefinitely until cancellation
                await Task.Delay(Timeout.Infinite, stoppingToken);
            }
            catch (TaskCanceledException) when (stoppingToken.IsCancellationRequested)
            {
                // Expected cancellation - proceed to shut down/cleanup below
            }
        }
        catch (Exception ex) when (!stoppingToken.IsCancellationRequested)
        {
            _logger?.LogError(ex, "Failed to advertise mDNS service");
        }
        finally
        {
            try
            {
                // Stop advertising and stop multicast service
                sd.Unadvertise(provisioningService);
                _logger?.LogInformation("mDNS service unadvertised.");
            }
            catch (Exception ex)
            {
                _logger?.LogWarning(ex, "Error while unadvertising mDNS service");
            }

            try
            {
                mdns.Stop();
                _logger?.LogDebug("mDNS multicast service stopped.");
            }
            catch (Exception ex)
            {
                _logger?.LogWarning(ex, "Error while unregistering mDNS service");
            }
        }
    }
}