using System;
using System.IO;
using System.Net;
using System.Net.Sockets;
using System.Threading;
using System.Text;
using System.Threading.Tasks;
using System.Text.RegularExpressions;
using System.Collections.Generic;

public class MiSTerRichPresence
{
    private const ushort DEFAULT_PORT = 41212;
    private const long CLIENT_ID = 864611729572495370;
    private const string CORENAME_FILE = "config/cores.txt";
    private const string GAMEFILTER_FILE = "config/filters.txt";

    private static Discord.Discord discord = null;
    private static Socket misterSocket = null;
    private static readonly byte[] buffer = new byte[1024];

    private static Dictionary<string, string> coreDict = null;
    private static List<Tuple<string, string>> filters = null;

    private static Socket StartClient(string serverAddress, ushort port)
    {
        Console.WriteLine("Connecting...");

        IPAddress ipAddress;
        try
        {
            ipAddress = IPAddress.Parse(serverAddress);
        }
        catch (FormatException)
        {
            IPHostEntry ipHostInfo = Dns.GetHostEntry(serverAddress);
            ipAddress = ipHostInfo.AddressList[0];
        }
        IPEndPoint remoteEP = new IPEndPoint(ipAddress, port);

        Socket client = new Socket(ipAddress.AddressFamily, SocketType.Stream, ProtocolType.Tcp);
        client.Connect(remoteEP);

        Console.WriteLine("Connected to MiSTer on {0}", client.RemoteEndPoint.ToString());

        return client;
    }

    private static Discord.Discord StartRichPresence()
    {
        var discord = new Discord.Discord(CLIENT_ID, (UInt64)Discord.CreateFlags.Default);

        discord.SetLogHook(Discord.LogLevel.Debug, (level, message) =>
        {
            Console.WriteLine("Log[{0}] {1}", level, message);
        });

        return discord;
    }

    private static string ProcessGameName(string gameName)
    {
        foreach (var filter in filters)
        {
            gameName = Regex.Replace(gameName, filter.Item1, filter.Item2);
            gameName = gameName.Trim();
        }

        return gameName;
    }

    private static void UpdateStatus(string statusString)
    {
        var statusStrings = statusString.Split('/');
        var coreName = statusStrings[0];
        var gameName = statusStrings[1];
        var elapsedString = statusStrings[2];
        long startTime = 0;

        var imageKey = coreName.ToLower();

        if (coreDict.ContainsKey(coreName))
        {
            coreName = coreDict[coreName];
        }

        if (gameName != "")
        {
            gameName = ProcessGameName(gameName);
        }

        if (elapsedString != "")
        {
            var elapsed = Convert.ToInt64(elapsedString);
            var currentTime = new DateTimeOffset(DateTime.UtcNow).ToUnixTimeSeconds();
            startTime = currentTime - elapsed;
        }

        UpdateActivity(gameName, coreName, startTime, imageKey);
    }

    private static void UpdateActivity(string game, string core, long startTime, string imageKey)
    {
        var activityManager = discord.GetActivityManager();

        var activity = new Discord.Activity
        {
            State = game,
            Details = core,
            Timestamps = new Discord.ActivityTimestamps
            {
                Start = startTime
            },
            Assets = new Discord.ActivityAssets
            {
                LargeImage = imageKey
            }
        };

        activityManager.UpdateActivity(activity, result =>
        {
            Console.WriteLine("Update Activity {0}", result);
        });
    }

    private static Dictionary<string,string> LoadCoreNames(string filename)
    {
        var dict = new Dictionary<string, string>();
        var lines = File.ReadAllLines(filename);
        foreach (var line in lines)
        {
            var values = ParseCsvRow(line);

            if (values.Count >= 2)
            {
                var coreName = values[0];
                var prettyCoreName = values[1];
                dict.Add(coreName, prettyCoreName);
            }
        }

        return dict;
    }

    private static List<Tuple<string, string>> LoadGameFilters(string filename)
    {
        var filters = new List<Tuple<string, string>>();
        var lines = File.ReadAllLines(filename);
        foreach (var line in lines)
        {
            var values = ParseCsvRow(line);

            if (values.Count >= 2)
            {
                var pattern = values[0];
                var replacement = values[1];
                filters.Add(new Tuple<string, string>(pattern, replacement));
            }
        }

        return filters;
    }

    private static int GetQuotedStringLength(string quoted)
    {
        if (!quoted.StartsWith('"')) return -1;

        var index = 1;
        while (index < quoted.Length && quoted[index] != '"')
        {
            index++;
            if (index + 1 < quoted.Length && quoted[index] == '"' && quoted[index + 1] == '"')
            {
                index += 2;
            }
        }

        if (index == quoted.Length) return -1;

        return index - 1;
    }
    
    private static string ParseQuotedString(string quoted)
    {
        var length = GetQuotedStringLength(quoted);
        if (length == -1) return null;

        var valueEscaped = quoted.Substring(1, length);
        var value = valueEscaped.Replace("\"\"", "\"");

        return value;
    }

    private static string ParseCsvValue(string csvValue, out int parseLength)
    {
        parseLength = 0;
        if (csvValue.StartsWith(','))
        {
            csvValue = csvValue.Substring(1);
            parseLength = 1;
        }

        if (csvValue.StartsWith('"'))
        {
            var value = ParseQuotedString(csvValue);
            if (value != null) parseLength += value.Length + 2;
            return value;
        }
        else
        {
            var endIndex = csvValue.IndexOf(',');
            if (endIndex == -1)
            {
                parseLength += csvValue.Length;
                return csvValue;
            }
            else
            {
                parseLength += endIndex;
                return csvValue.Substring(0, endIndex);
            }
        }
    }

    private static List<string> ParseCsvRow(string csvRow)
    {
        var values = new List<string>();
        while (csvRow.Length > 0)
        {
            values.Add(ParseCsvValue(csvRow, out int length));
            if (length <= 0) break;
            csvRow = csvRow.Substring(length);
        }

        return values;
    }

    private static void PrintUsageAndExit()
    {
        Console.WriteLine("Usage: ");
        Console.WriteLine("  ./misterStatus.exe <MiSTerIP> [PORT]");
        Environment.Exit(0);
    }

    private static void CheckArguments(string[] args)
    {
        if (args.Length < 1 || args.Length > 2)
        {
            PrintUsageAndExit();
        }
        if (args.Length == 2)
        {
            try
            {
                ushort.Parse(args[1]);
            }
            catch (FormatException)
            {
                Console.WriteLine("Error: Port not formatted correctly");
                PrintUsageAndExit();
            }
        }
    }

    private static void Initialise(string[] args)
    {
        discord = StartRichPresence();
        try
        {
            ushort port = DEFAULT_PORT;
            if (args.Length == 2)
            {
                port = ushort.Parse(args[1]);
            }
            misterSocket = StartClient(args[0], port);
        }
        catch (SocketException e)
        {
            Console.WriteLine("Failed to connect to MiSTer: {0}", e.Message);
            return;
        }

        coreDict = LoadCoreNames(CORENAME_FILE);
        filters = LoadGameFilters(GAMEFILTER_FILE);

        AppDomain.CurrentDomain.ProcessExit += new EventHandler(OnProcessExit);
    }

    private static void ProcessStatus(Task<int> statusTask)
    {
        try
        {
            int bytesRec = statusTask.Result;

            var status = Encoding.ASCII.GetString(buffer, 0, bytesRec);
            Console.WriteLine("Status received: {0}", status);

            UpdateStatus(status);
        }
        catch (Exception e)
        {
            Console.WriteLine("Error receiving status: " + e.Message);
            Environment.Exit(1);
        }
    }

    public static void Main(string[] args)
    {
        CheckArguments(args);
        Initialise(args);

        var receiveTask = misterSocket.ReceiveAsync(buffer, SocketFlags.None);

        while (true)
        {
            if (receiveTask.IsCompleted)
            {
                ProcessStatus(receiveTask);
                receiveTask = misterSocket.ReceiveAsync(buffer, SocketFlags.None);
            }
            try
            {
                discord.RunCallbacks();
            }
            catch (Discord.ResultException)
            {
                Console.WriteLine("Discord has been closed, closing app");
                Environment.Exit(0);
            }
            Thread.Sleep(50);
        }
    }

    private static void OnProcessExit(object sender, EventArgs e)
    {
        if (misterSocket != null)
        {
            misterSocket.Close();
        }
        if (discord != null)
        {
            try
            {
                discord.Dispose();
            }
            catch (Exception) {}
        }
    }
}
