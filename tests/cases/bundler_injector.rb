$stdout.sync = true

require "bundler/injector"

result = Bundler::Injector.inject("source \"https://rubygems.org\"\n", ["alpha", "beta"])
puts result.include?("gem \"alpha\"")
puts result.include?("gem \"beta\"")
puts result.lines.length
