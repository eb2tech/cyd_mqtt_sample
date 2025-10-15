using System.Net.Sockets;
using System.Text;
using Microsoft.Extensions.Hosting;
using Microsoft.Extensions.Logging;
using Microsoft.Extensions.Options;

namespace TokenProvisioningService;

public class UdpListenerService : BackgroundService
{
    private readonly ILogger<UdpListenerService> _logger;
    private readonly TokenProvisioningOptions _options;
    private readonly ITokenService _tokenService;
    private readonly IDeviceDatabase _deviceDatabase;

    public UdpListenerService(
        ILogger<UdpListenerService> logger,
        IOptions<TokenProvisioningOptions> options,
        ITokenService tokenService,
        IDeviceDatabase deviceDatabase)
    {
        _logger = logger;
        _options = options.Value;
        _tokenService = tokenService;
        _deviceDatabase = deviceDatabase;
    }

    protected override async Task ExecuteAsync(CancellationToken stoppingToken)
    {
        _logger.LogInformation("UDP Listener starting on port {Port}", _options.UdpPort);

        using var udpClient = new UdpClient(_options.UdpPort);
        
        while (!stoppingToken.IsCancellationRequested)
        {
            try
            {
                var result = await udpClient.ReceiveAsync(stoppingToken);
                _ = Task.Run(() => ProcessRequestAsync(udpClient, result), stoppingToken);
            }
            catch (Exception ex) when (!stoppingToken.IsCancellationRequested)
            {
                _logger.LogError(ex, "Error receiving UDP message");
                await Task.Delay(1000, stoppingToken);
            }
        }
    }

    private async Task ProcessRequestAsync(UdpClient udpClient, UdpReceiveResult result)
    {
        try
        {
            var message = Encoding.UTF8.GetString(result.Buffer);
            _logger.LogDebug("Received UDP message from {Endpoint}: {Message}", result.RemoteEndPoint, message);

            // TODO: Parse request, validate device, generate token, send response
            var response = "Token response placeholder";
            var responseBytes = Encoding.UTF8.GetBytes(response);
            
            await udpClient.SendAsync(responseBytes, result.RemoteEndPoint);
            _logger.LogDebug("Sent response to {Endpoint}", result.RemoteEndPoint);
        }
        catch (Exception ex)
        {
            _logger.LogError(ex, "Error processing UDP request from {Endpoint}", result.RemoteEndPoint);
        }
    }
}