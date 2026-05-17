env = { "STONED_DEMO_ENV" => "xyz" }
puts system(env, "sh", "-c", "printf '%s' \"$STONED_DEMO_ENV\"")
puts system("sh", "-c", "printf '%s|%s' \"$0\" \"$1\"", "shellname", "alpha beta")
