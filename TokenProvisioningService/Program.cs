/*
  Token Provisioning Service for CYD MQTT Devices
  -----------------------------------------------
  This service runs on a Raspberry Pi and listens for UDP requests from CYD devices.
  Each request includes a UUID that identifies the device.

  Responsibilities:
  - Validate incoming UUIDs
  - Generate short-lived JWT tokens for MQTT authentication
  - Store token metadata in LiteDB
  - Respond with token and broker info
  - Optionally log onboarding events

  Future:
  - Integrate with Mosquitto plugin for token validation
  - Add mDNS responder for broker discovery
*/

using System.Net;
using System.Text.Json;
using Microsoft.Extensions.Configuration;
using Microsoft.Extensions.DependencyInjection;
using Microsoft.Extensions.Hosting;
using Microsoft.Extensions.Logging;
using Microsoft.IdentityModel.Tokens;
using System.IdentityModel.Tokens.Jwt;
using System.Security.Claims;
using System.Security.Cryptography;
using LiteDB;
using TokenProvisioningService;

var host = Host.CreateDefaultBuilder(args)
               .ConfigureAppConfiguration((context, config) =>
               {
                   config.AddJsonFile("appsettings.json", optional: true, reloadOnChange: true);
                   config.AddJsonFile($"appsettings.{context.HostingEnvironment.EnvironmentName}.json",
                                      optional: true, reloadOnChange: true);
                   config.AddEnvironmentVariables();
                   config.AddCommandLine(args);
               })
               .ConfigureServices((context, services) =>
               {
                   // Register configuration sections
                   services.Configure<TokenProvisioningOptions>(
                       context.Configuration.GetSection("TokenProvisioning"));
                   services.Configure<MqttBrokerOptions>(
                       context.Configuration.GetSection("MqttBroker"));

                   // Register services
                   services.AddSingleton<ITokenService, TokenService>();
                   services.AddSingleton<IDeviceDatabase, DeviceDatabase>();
                   services.AddHostedService<UdpListenerService>();
               })
               .ConfigureLogging((context, logging) =>
               {
                   logging.ClearProviders();
                   logging.AddConsole();
                   logging.AddDebug();

                   if (context.HostingEnvironment.IsDevelopment())
                   {
                       logging.SetMinimumLevel(LogLevel.Debug);
                   }
                   else
                   {
                       logging.SetMinimumLevel(LogLevel.Information);
                   }
               })
               .Build();

var logger = host.Services.GetRequiredService<ILoggerFactory>().CreateLogger("TokenProvisioningService.Program");
logger.LogInformation("Token Provisioning Service starting...");

try
{
    await host.RunAsync();
}
catch (Exception ex)
{
    logger.LogCritical(ex, "Application terminated unexpectedly");
}
finally
{
    logger.LogInformation("Token Provisioning Service stopped");
}
