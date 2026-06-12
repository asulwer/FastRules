import re

with open('tests/test_error_paths.cpp', 'r') as f:
    content = f.read()

# Pattern 1: Rule x; x.id = N; x.dependsOnRuleName = "ruleM";
# Need to add: x.name = "ruleN";
content = re.sub(
    r'Rule ([a-z]); \1\.id = (\d+); \1\.dependsOnRuleName = "rule(\d+)";',
    r'Rule \1; \1.id = \2; \1.name = "rule\2"; \1.dependsOnRuleName = "rule\3";',
    content
)

# Pattern 2: Rule x; x.id = N; // No dependency
# Need to add: x.name = "ruleN";
content = re.sub(
    r'Rule ([a-z]); \1\.id = (\d+); // No dependency',
    r'Rule \1; \1.id = \2; \1.name = "rule\2"; // No dependency',
    content
)

# Pattern 3: Rule x; x.id = N;
# (when no dependsOnRuleName)
content = re.sub(
    r'Rule ([a-z]); \1\.id = (\d+);\s*$',
    r'Rule \1; \1.id = \2; \1.name = "rule\2";',
    content
)

with open('tests/test_error_paths.cpp', 'w') as f:
    f.write(content)

print("Fixed test_error_paths.cpp")
