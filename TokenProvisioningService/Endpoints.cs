using System.Text.Json;
using Microsoft.Extensions.Options;

namespace TokenProvisioningService;

public static class Endpoints
{
    public static void MapEndpoints(this WebApplication app)
    {
        MapProvisionEndpoints(app);
    }

    private static void MapProvisionEndpoints(this WebApplication app)
    {
        app.MapPost("/provision", async (HttpContext context, ITokenService tokenService,
                                         IOptions<MqttBrokerOptions> mqttOptions, ILoggerFactory loggerFactory) =>
        {
            var logger = loggerFactory.CreateLogger("ProvisionEndpoint");

            try
            {
                // Parse JSON request body
                using var doc = await JsonDocument.ParseAsync(context.Request.Body, cancellationToken: context.RequestAborted);
                var root = doc.RootElement;

                // Extract fields with safe defaults
                var deviceId = root.TryGetProperty("device_id", out var pj) && pj.ValueKind == JsonValueKind.String
                    ? pj.GetString()!
                    : string.Empty;
                var deviceType = root.TryGetProperty("device_type", out var pd) && pd.ValueKind == JsonValueKind.String
                    ? pd.GetString()!
                    : string.Empty;
                var macAddress = root.TryGetProperty("mac_address", out var pm) && pm.ValueKind == JsonValueKind.String
                    ? pm.GetString()!
                    : string.Empty;
                var requestType = root.TryGetProperty("request_type", out var pr) && pr.ValueKind == JsonValueKind.String
                    ? pr.GetString()!
                    : string.Empty;

                if (string.IsNullOrWhiteSpace(deviceId) || string.IsNullOrWhiteSpace(deviceType) ||
                    string.IsNullOrWhiteSpace(macAddress) || string.IsNullOrWhiteSpace(requestType))
                {
                    context.Response.StatusCode = StatusCodes.Status400BadRequest;
                    await context.Response.WriteAsJsonAsync(
                        new { status = "error", error = "Missing required fields: device_id, device_type, mac_address, and request_type" },
                        context.RequestAborted);
                    return;
                }

                if (requestType != "mqtt_config")
                {
                    context.Response.StatusCode = StatusCodes.Status406NotAcceptable;
                    await context.Response.WriteAsJsonAsync(new { status = "error", error = "Unsupported request_type" },
                                                            context.RequestAborted);
                    return;
                }

                logger.LogDebug("Provision request {Request} for device {DeviceId} mac {MacAddress} type {DeviceType}",
                                requestType, deviceId, macAddress, deviceType);

                // Validate device (placeholder logic in TokenService)
                var valid = await tokenService.ValidateDeviceAsync(deviceId, deviceType);
                if (!valid)
                {
                    context.Response.StatusCode = StatusCodes.Status403Forbidden;
                    await context.Response.WriteAsJsonAsync(new { status = "error", error = "Device validation failed" },
                                                            context.RequestAborted);
                    return;
                }

                // Generate token
                var token = await tokenService.GenerateTokenAsync(deviceId, deviceType);

                // Build response
                var response = new
                {
                    status = "success",
                    mqtt_broker = mqttOptions.Value.Host,
                    mqtt_port = mqttOptions.Value.Port,
                    mqtt_username = "cyd",
                    mqtt_password = token
                };
                context.Response.ContentType = "application/json";

                logger.LogDebug("Responding with broker {Broker}:{BrokerPort}", response.mqtt_broker, response.mqtt_port);

                await context.Response.WriteAsJsonAsync(response, context.RequestAborted);
            }
            catch (JsonException jex)
            {
                logger.LogWarning(jex, "Invalid JSON in /provision request");
                context.Response.StatusCode = StatusCodes.Status400BadRequest;
                await context.Response.WriteAsJsonAsync(new { status = "error", error = "Invalid JSON" },
                                                        context.RequestAborted);
            }
            catch (OperationCanceledException) when (context.RequestAborted.IsCancellationRequested)
            {
                // Client cancelled - nothing to do
            }
            catch (Exception ex)
            {
                logger.LogError(ex, "Unexpected error handling /provision");
                context.Response.StatusCode = StatusCodes.Status500InternalServerError;
                await context.Response.WriteAsJsonAsync(new { status = "error", error = "Internal server error" },
                                                        context.RequestAborted);
            }
        });
    }
}
