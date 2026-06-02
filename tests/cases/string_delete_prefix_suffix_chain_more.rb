path = "src/runtime/eval_dispatch.c"

puts path.delete_prefix("src/")
puts path.delete_suffix(".c")
puts "preview".delete_prefix("pre").delete_suffix("iew")
p "ruby".delete_prefix("stoned")
p "ruby".delete_suffix("runtime")
