using System;
using System.Runtime.InteropServices;
using System.Text.Json;
using System.Text.Json.Serialization;
using System.Collections.Generic;

// FastRules C# Example
// This example uses the FastRules C API via P/Invoke.
// The C API is located in: examples/c_api/
// Build the C API shared library first before running this example.

namespace FastRulesExample
{
    /// <summary>
    /// C# P/Invoke wrapper for FastRules C API
    /// </summary>
    public class FastRulesEngine : IDisposable
    {
        private IntPtr _engine;
        private bool _disposed;

        /// <summary>
        /// Internal handle for use by Workflow class.
        /// </summary>
        internal IntPtr Handle => _engine;

        // P/Invoke declarations - import from fastrules_c_api.dll
        [DllImport("fastrules_c_api", CallingConvention = CallingConvention.Cdecl)]
        private static extern IntPtr fastrules_engine_create();

        [DllImport("fastrules_c_api", CallingConvention = CallingConvention.Cdecl)]
        private static extern void fastrules_engine_destroy(IntPtr engine);

        [DllImport("fastrules_c_api", CallingConvention = CallingConvention.Cdecl)]
        private static extern IntPtr fastrules_engine_get_last_error(IntPtr engine);

        [DllImport("fastrules_c_api", CallingConvention = CallingConvention.Cdecl)]
        private static extern IntPtr fastrules_workflow_create(IntPtr engine, int id, string description);

        [DllImport("fastrules_c_api", CallingConvention = CallingConvention.Cdecl)]
        private static extern int fastrules_workflow_add_rule(IntPtr engine, IntPtr workflow, int id, 
            string name, string expression, string action, string description, bool isActive);

        [DllImport("fastrules_c_api", CallingConvention = CallingConvention.Cdecl)]
        private static extern int fastrules_workflow_set_rule_priority(IntPtr engine, IntPtr workflow, 
            int rule_id, int priority);

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

        /// <summary>
        /// Create a workflow in-memory (no JSON required).
        /// </summary>
        public Workflow CreateWorkflow(int id, string description = "")
        {
            var workflowPtr = fastrules_workflow_create(_engine, id, description);
            if (workflowPtr == IntPtr.Zero)
            {
                throw new InvalidOperationException($"Failed to create workflow: {GetLastError()}");
            }

            return new Workflow(this, workflowPtr);
        }

        /// <summary>
        /// Load a workflow from JSON (legacy method).
        /// </summary>
        [Obsolete("Use CreateWorkflow with AddRule instead")]
        public Workflow LoadWorkflow(string json)
        {
            throw new NotImplementedException("JSON loading requires JSON extension. Use CreateWorkflow with AddRule instead.");
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

        /// <summary>
        /// Add a rule to the workflow.
        /// </summary>
        public void AddRule(int id, string expression, string name = null, string action = null, string description = null, bool isActive = true)
        {
            var result = fastrules_workflow_add_rule(_engine.Handle, _workflow, id, name, expression, action, description, isActive);
            if (result != 0)
            {
                throw new InvalidOperationException($"Failed to add rule: {_engine.GetLastError()}");
            }
        }

        /// <summary>
        /// Set rule priority.
        /// </summary>
        public void SetRulePriority(int ruleId, int priority)
        {
            var result = fastrules_workflow_set_rule_priority(_engine.Handle, _workflow, ruleId, priority);
            if (result != 0)
            {
                throw new InvalidOperationException($"Failed to set rule priority: {_engine.GetLastError()}");
            }
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
            // Convert dictionary to "key=value;key2=value2" format
            var paramParts = new List<string>();
            foreach (var kvp in parameters)
            {
                string valueStr = kvp.Value switch
                {
                    bool b => b ? "true" : "false",
                    _ => kvp.Value?.ToString() ?? ""
                };
                paramParts.Add($"{kvp.Key}={valueStr}");
            }
            string paramsStr = string.Join(";", paramParts);
            
            IntPtr resultsPtr;

            var result = _engine.ExecuteWorkflow(_workflow, paramsStr, out resultsPtr);
            if (result != 0)
            {
                throw new InvalidOperationException($"Failed to execute workflow: {_engine.GetLastError()}");
            }

            try
            {
                var resultStr = Marshal.PtrToStringAnsi(resultsPtr);
                return ParseResults(resultStr);
            }
            finally
            {
                FastRulesEngine.FreeResults(resultsPtr);
            }
        }

        public List<RuleResult> ExecuteParallel(Dictionary<string, object> parameters)
        {
            // Convert dictionary to "key=value;key2=value2" format
            var paramParts = new List<string>();
            foreach (var kvp in parameters)
            {
                string valueStr = kvp.Value switch
                {
                    bool b => b ? "true" : "false",
                    _ => kvp.Value?.ToString() ?? ""
                };
                paramParts.Add($"{kvp.Key}={valueStr}");
            }
            string paramsStr = string.Join(";", paramParts);
            
            IntPtr resultsPtr;

            var result = _engine.ExecuteWorkflowParallel(_workflow, paramsStr, out resultsPtr);
            if (result != 0)
            {
                throw new InvalidOperationException($"Failed to execute workflow in parallel: {_engine.GetLastError()}");
            }

            try
            {
                var resultStr = Marshal.PtrToStringAnsi(resultsPtr);
                return ParseResults(resultStr);
            }
            finally
            {
                FastRulesEngine.FreeResults(resultsPtr);
            }
        }

        /// <summary>
        /// Parse result string (format: "id1:name1:success1:error1;id2:name2:success2:error2")
        /// </summary>
        private List<RuleResult> ParseResults(string resultStr)
        {
            var results = new List<RuleResult>();
            if (string.IsNullOrEmpty(resultStr))
                return results;

            var ruleParts = resultStr.Split(';');
            foreach (var part in ruleParts)
            {
                if (string.IsNullOrWhiteSpace(part))
                    continue;

                var fields = part.Split(':');
                // Format: id:name:success:error
                if (fields.Length >= 3 && int.TryParse(fields[0], out int ruleId))
                {
                    string ruleName = fields[1];
                    if (int.TryParse(fields[2], out int successInt))
                    {
                        results.Add(new RuleResult
                        {
                            RuleId = ruleId,
                            RuleName = ruleName,
                            Success = successInt == 1,
                            Error = fields.Length > 3 ? fields[3] : null
                        });
                    }
                }
            }
            return results;
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
        public int RuleId { get; set; }
        public string RuleName { get; set; }
        public bool Success { get; set; }
        public string Error { get; set; }
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

                // Example 4: Parent-child rules
                RunParentChildExample(engine);
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

            // Create workflow in-memory (no JSON)
            using var workflow = engine.CreateWorkflow(1, "Customer Validation");
            
            // Add rules programmatically
            workflow.AddRule(1, "age >= 18", description: "Age check");
            workflow.AddRule(2, "string.len(name) > 0", description: "Name check");
            
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

            // Create workflow in-memory (no JSON)
            using var workflow = engine.CreateWorkflow(2, "Order Processing");
            
            // Add rules programmatically
            workflow.AddRule(1, "orderTotal >= 10.0", description: "Minimum order");
            workflow.AddRule(2, "isVip == true", description: "VIP bonus");
            
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

            // Create workflow in-memory (no JSON)
            using var workflow = engine.CreateWorkflow(3, "Parallel Rules");
            
            // Add rules programmatically
            workflow.AddRule(1, "x > 0");
            workflow.AddRule(2, "y > 0");
            workflow.AddRule(3, "z > 0");
            workflow.AddRule(4, "w > 0");
            
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

        static void RunParentChildExample(FastRulesEngine engine)
        {
            Console.WriteLine("Example 4: Parent-Child Rules");
            Console.WriteLine("-----------------------------");

            // Create workflow in-memory with parent-child rule hierarchy
            using var workflow = engine.CreateWorkflow(4, "Parent-Child Example");
            
            // Add child rules (execute first)
            workflow.AddRule(1, "age >= 18", name: "age-check", description: "Age validation");
            workflow.AddRule(2, "len(name) > 0", name: "name-check", description: "Name validation");
            
            // Add parent rule that references children via context
            workflow.AddRule(3, "context.getResult(1).success == true and context.getResult(2).success == true", 
                            name: "parent-validation", 
                            description: "Parent checks all children passed");
            
            workflow.Compile();

            // Test: Valid adult with name
            Console.WriteLine("\nTest: Valid customer (Alice, age 25)");
            var results = workflow.Execute(new Dictionary<string, object>
            {
                ["age"] = 25,
                ["name"] = "Alice"
            });
            PrintResultsWithNames(results);

            // Test: Minor (age check fails)
            Console.WriteLine("\nTest: Minor (Bob, age 15)");
            results = workflow.Execute(new Dictionary<string, object>
            {
                ["age"] = 15,
                ["name"] = "Bob"
            });
            PrintResultsWithNames(results);

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

        static void PrintResultsWithNames(List<RuleResult> results)
        {
            foreach (var result in results)
            {
                var status = result.Success ? "PASS" : "FAIL";
                var name = string.IsNullOrEmpty(result.RuleName) ? $"Rule {result.RuleId}" : result.RuleName;
                Console.WriteLine($"  {name} (ID {result.RuleId}): {status}");
                if (!string.IsNullOrEmpty(result.Error))
                {
                    Console.WriteLine($"    Error: {result.Error}");
                }
            }
        }
    }
}
