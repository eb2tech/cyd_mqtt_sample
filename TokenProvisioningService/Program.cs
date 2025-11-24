using TokenProvisioningService;

var builder = WebApplication.CreateBuilder(args);

builder.Services.AddSingleton<IDeviceDatabase, DeviceDatabase>();
builder.Services.AddSingleton<ITokenService, TokenService>();
builder.Services.AddHostedService<MdnsAdvertiserService>();
builder.Services.Configure<MqttBrokerOptions>(builder.Configuration.GetSection(nameof(MqttBrokerOptions)));
builder.Services.Configure<TokenProvisioningOptions>(builder.Configuration.GetSection(nameof(TokenProvisioningOptions)));

builder.WebHost.ConfigureKestrel((context, options) =>
{
    var port = context.Configuration.GetValue<int>("TokenProvisioningOptions:Port", 12345);
    options.ListenAnyIP(port); // listen on 0.0.0.0:port for IPv4/IPv6
});

var app = builder.Build();

// Map endpoints from extension methods
app.MapEndpoints();

app.Run();