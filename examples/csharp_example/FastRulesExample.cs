using System;
using System.Runtime.InteropServices;
using System.Text.Json;
using System.Text.Json.Serialization;
using System.Collections.Generic;

namespace FastRulesExample
{
    /// <summary>
    /// C# P/Invoke wrapper for FastRules C API
    /// </summary>
    public class FastRulesEngine : IDisposable
    {
        private IntPtr _engine;
        private bool _disposed;

        // P/Invoke declarations - import from fastrules_c_api.dll
        [DllImport("fastrules_c_api", CallingConvention = CallingConvention.Cdecl)]
        private static extern IntPtr fastrules_engine_create();

        [DllImport("fastrules_c_api", CallingConvention = CallingConvention.Cdecl)]
        private static extern void fastrules_engine_destroy(IntPtr engine);

        [DllImport("fastrules_c_api", CallingConvention = CallingConvention.Cdecl)]
        private static extern IntPtr fastrules_engine_get_last_error(IntPtr engine);

        [DllImport("fastrules_c_api", CallingConvention = CallingConvention.Cdecl)]
        private static extern IntPtr fastrules_workflow_create_from_json(IntPtr engine, string json);

        [DllImport("fastrules_c_api", CallingConvention = CallingConvention.Cdecl)]
        private static extern void fastrules_workflow_destroy(IntPtr workflow);

        [DllImport("fastrules_c_api", CallingConvention = CallingConvention.Cdecl)]
        private static extern int fastrules_workflow_compile(IntPtr engine, IntPtr workflow);

        [DllImport("fastrules_c_api", CallingConvention = CallingConvention.Cdecl)]
        private static extern int fastrules_workflow_execute(IntPtr engine, IntPtr workflow, string jsonParams, out IntPtr results);

        [DllImport("fastrules_c_api", CallingConvention = CallingConvention.Cdecl)]
        private static extern int fastrules_workflow_execute_parallel(IntPtr engine, IntPtr workflow, string jsonParams, out IntPtr results);

        [DllImport("fastrules_c_api", CallingConvention = CallingConvention.Cdecl)]
        private static extern void fastrules_free(IntPtr ptr);

        [DllImport("fastrules_c_api", CallingConvention = CallingConvention.Cdecl)]
        private static extern IntPtr fastrules_get_version();

        [DllImport("fastrules_c_api", CallingConvention = CallingConvention.Cdecl)]
        private static extern bool fastrules_validate_workflow_json(string json);

        // Error codes
        private const int FASTRULES_OK = 0;
        private const int FASTRULES_ERROR_NULL_PTR = -1;
        private const int FASTRULES_ERROR_INVALID_JSON = -2;
        private const int FASTRULES_ERROR_COMPILATION_FAILED = -3;
        private const int FASTRULES_ERROR_EXECUTION_FAILED = -4;
        private const int FASTRULES_ERROR_MEMORY = -5;
        private const int FASTRULES_ERROR_UNKNOWN = -99;

        public FastRulesEngine()
        {
            _engine = fastrules_engine_create();
            if (_engine == IntPtr.Zero)
            {
                throw new InvalidOperationException("Failed to create FastRules engine");
            }
        }

        public string Version
        {
            get
            {
                var ptr = fastrules_get_version();
                return Marshal.PtrToStringAnsi(ptr);
            }
        }

        public Workflow LoadWorkflow(string json)
        {
            if (!fastrules_validate_workflow_json(json))
            {
                throw new ArgumentException("Invalid workflow JSON");
            }

            var workflowPtr = fastrules_workflow_create_from_json(_engine, json);
            if (workflowPtr == IntPtr.Zero)
            {
                throw new InvalidOperationException($"Failed to load workflow: {GetLastError()}");
            }

            return new Workflow(this, workflowPtr);
        }

        internal string GetLastError()
        {
            var ptr = fastrules_engine_get_last_error(_engine);
            return Marshal.PtrToStringAnsi(ptr) ?? "Unknown error";
        }

        internal static void FreeResults(IntPtr results)
        {
            fastrules_free(results);
        }

        internal int CompileWorkflow(IntPtr workflow)
        {
            return fastrules_workflow_compile(_engine, workflow);
        }

        internal int ExecuteWorkflow(IntPtr workflow, string jsonParams, out IntPtr results)
        {
            return fastrules_workflow_execute(_engine, workflow, jsonParams, out results);
        }

        internal int ExecuteWorkflowParallel(IntPtr workflow, string jsonParams, out IntPtr results)
        {
            return fastrules_workflow_execute_parallel(_engine, workflow, jsonParams, out results);
        }

        public void Dispose()
        {
            if (!_disposed)
            {
                fastrules_engine_destroy(_engine);
                _engine = IntPtr.Zero;
                _disposed = true;
            }
        }
    }

    /// <summary>
    /// Represents a FastRules workflow
    /// </summary>
    public class Workflow : IDisposable
    {
        private readonly FastRulesEngine _engine;
        private readonly IntPtr _workflow;
        private bool _disposed;

        internal Workflow(FastRulesEngine engine, IntPtr workflow)
        {
            _engine = engine;
            _workflow = workflow;
        }

        public void Compile()
        {
            var result = _engine.CompileWorkflow(_workflow);
            if (result != 0)
            {
                throw new InvalidOperationException($"Failed to compile workflow: {_engine.GetLastError()}");
            }
        }

        public List<RuleResult> Execute(Dictionary<string, object> parameters)
        {
            var jsonParams = JsonSerializer.Serialize(parameters);
            IntPtr resultsPtr;

            var result = _engine.ExecuteWorkflow(_workflow, jsonParams, out resultsPtr);
            if (result != 0)
            {
                throw new InvalidOperationException($"Failed to execute workflow: {_engine.GetLastError()}");
            }

            try
            {
                var json = Marshal.PtrToStringAnsi(resultsPtr);
                return JsonSerializer.Deserialize<List<RuleResult>>(json);
            }
            finally
            {
                FastRulesEngine.FreeResults(resultsPtr);
            }
        }

        public List<RuleResult> ExecuteParallel(Dictionary<string, object> parameters)
        {
            var jsonParams = JsonSerializer.Serialize(parameters);
            IntPtr resultsPtr;

            var result = _engine.ExecuteWorkflowParallel(_workflow, jsonParams, out resultsPtr);
            if (result != 0)
            {
                throw new InvalidOperationException($"Failed to execute workflow in parallel: {_engine.GetLastError()}");
            }

            try
            {
                var json = Marshal.PtrToStringAnsi(resultsPtr);
                return JsonSerializer.Deserialize<List<RuleResult>>(json);
            }
            finally
            {
                FastRulesEngine.FreeResults(resultsPtr);
            }
        }

        public void Dispose()
        {
            if (!_disposed)
            {
                // Note: workflow destruction is handled by the engine
                _disposed = true;
            }
        }
    }

    /// <summary>
    /// Represents a rule execution result
    /// </summary>
    public class RuleResult
    {
        [JsonPropertyName("ruleId")]
        public int RuleId { get; set; }

        [JsonPropertyName("success")]
        public bool Success { get; set; }

        [JsonPropertyName("error")]
        public string Error { get; set; }

        [JsonPropertyName("actionResult")]
        public string ActionResult { get; set; }
    }

    /// <summary>
    /// Example program demonstrating FastRules usage from C#
    /// </summary>
    class Program
    {
        static void Main(string[] args)
        {
            Console.WriteLine("FastRules C# Example");
            Console.WriteLine("====================\n");

            try
            {
                using var engine = new FastRulesEngine();
                Console.WriteLine($"FastRules Version: {engine.Version}\n");

                // Example 1: Basic validation
                RunBasicValidationExample(engine);

                // Example 2: Order processing with actions
                RunOrderProcessingExample(engine);

                // Example 3: Parallel execution
                RunParallelExecutionExample(engine);
            }
            catch (DllNotFoundException)
            {
                Console.WriteLine("Error: Could not load fastrules.dll");
                Console.WriteLine("Please ensure the FastRules shared library is built and in your PATH.");
                Console.WriteLine("Build with: cmake -B build -DFASTRULES_BUILD_SHARED=ON");
            }
            catch (Exception ex)
            {
                Console.WriteLine($"Error: {ex.Message}");
            }
        }

        static void RunBasicValidationExample(FastRulesEngine engine)
        {
            Console.WriteLine("Example 1: Basic Customer Validation");
            Console.WriteLine("------------------------------------");

            var workflowJson = @"
            {
                ""id"": 1,
                ""description"": ""Customer Validation"",
                ""rules"": [
                    {
                        ""id"": 1,
                        ""description"": ""Age check"",
                        ""expression"": ""age >= 18""
                    },
                    {
                        ""id"": 2,
                        ""description"": ""Name check"",
                        ""expression"": ""string.len(name) > 0""
                    }
                ]
            }";

            using var workflow = engine.LoadWorkflow(workflowJson);
            workflow.Compile();

            // Test case 1: Valid adult
            Console.WriteLine("\nTest 1: Adult customer (Alice, age 25)");
            var results = workflow.Execute(new Dictionary<string, object>
            {
                ["age"] = 25,
                ["name"] = "Alice"
            });
            PrintResults(results);

            // Test case 2: Minor
            Console.WriteLine("\nTest 2: Minor customer (Bob, age 15)");
            results = workflow.Execute(new Dictionary<string, object>
            {
                ["age"] = 15,
                ["name"] = "Bob"
            });
            PrintResults(results);

            Console.WriteLine();
        }

        static void RunOrderProcessingExample(FastRulesEngine engine)
        {
            Console.WriteLine("Example 2: Order Processing");
            Console.WriteLine("---------------------------");

            var workflowJson = @"
            {
                ""id"": 2,
                ""description"": ""Order Processing"",
                ""rules"": [
                    {
                        ""id"": 1,
                        ""description"": ""Minimum order"",
                        ""expression"": ""orderTotal >= 10.0""
                    },
                    {
                        ""id"": 2,
                        ""description"": ""VIP bonus"",
                        ""expression"": ""isVip == true""
                    }
                ]
            }";

            using var workflow = engine.LoadWorkflow(workflowJson);
            workflow.Compile();

            // Test: Standard order
            Console.WriteLine("\nTest: Standard order ($50, non-VIP)");
            var results = workflow.Execute(new Dictionary<string, object>
            {
                ["orderTotal"] = 50.0,
                ["isVip"] = false
            });
            PrintResults(results);

            // Test: VIP order
            Console.WriteLine("\nTest: VIP order ($50, VIP)");
            results = workflow.Execute(new Dictionary<string, object>
            {
                ["orderTotal"] = 50.0,
                ["isVip"] = true
            });
            PrintResults(results);

            Console.WriteLine();
        }

        static void RunParallelExecutionExample(FastRulesEngine engine)
        {
            Console.WriteLine("Example 3: Parallel Execution");
            Console.WriteLine("-----------------------------");

            var workflowJson = @"
            {
                ""id"": 3,
                ""description"": ""Parallel Rules"",
                ""rules"": [
                    { ""id"": 1, ""expression"": ""x > 0"" },
                    { ""id"": 2, ""expression"": ""y > 0"" },
                    { ""id"": 3, ""expression"": ""z > 0"" },
                    { ""id"": 4, ""expression"": ""w > 0"" }
                ]
            }";

            using var workflow = engine.LoadWorkflow(workflowJson);
            workflow.Compile();

            Console.WriteLine("\nSequential Execution:");
            var sw = System.Diagnostics.Stopwatch.StartNew();
            var results = workflow.Execute(new Dictionary<string, object>
            {
                ["x"] = 1, ["y"] = 2, ["z"] = 3, ["w"] = 4
            });
            sw.Stop();
            PrintResults(results);
            Console.WriteLine($"  Time: {sw.Elapsed.TotalMilliseconds:F3} ms");

            Console.WriteLine("\nParallel Execution:");
            sw.Restart();
            results = workflow.ExecuteParallel(new Dictionary<string, object>
            {
                ["x"] = 1, ["y"] = 2, ["z"] = 3, ["w"] = 4
            });
            sw.Stop();
            PrintResults(results);
            Console.WriteLine($"  Time: {sw.Elapsed.TotalMilliseconds:F3} ms");

            Console.WriteLine();
        }

        static void PrintResults(List<RuleResult> results)
        {
            foreach (var result in results)
            {
                var status = result.Success ? "PASS" : "FAIL";
                Console.WriteLine($"  Rule {result.RuleId}: {status}");
                if (!string.IsNullOrEmpty(result.Error))
                {
                    Console.WriteLine($"    Error: {result.Error}");
                }
            }
        }
    }
}
