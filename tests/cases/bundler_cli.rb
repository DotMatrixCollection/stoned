$stdout.sync = true

require "bundler/cli"

cli = Bundler::CLI.start(["install", "--path", "vendor/bundle"])
puts cli.is_a?(Bundler::CLI)
puts cli.args[0]
puts cli.args[2]
puts cli.install
puts cli.exec
