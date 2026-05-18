$stdout.sync = true

require "bundler/injector"

result = Bundler::Injector.inject("source \"https://rubygems.org\"\n", ["alpha", "beta"])
puts result.include?("gem \"alpha\"")
puts result.include?("gem \"beta\"")
puts result.lines.length

result2 = Bundler::Injector.inject(result, [
  {name: "beta"},
  {name: "gamma", version: "~> 1.0", group: :tools, require: false},
  {name: "delta", path: "../delta", groups: [:dev, :test]}
])
puts result2.scan("gem \"beta\"").length
puts result2.include?("gem \"gamma\", \"~> 1.0\", group: \"tools\", require: false")
puts result2.include?("gem \"delta\", groups: [\"dev\", \"test\"], path: \"../delta\"")
