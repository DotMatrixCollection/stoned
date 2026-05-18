$stdout.sync = true

require "bundler/cli/common"

old_without = ENV["BUNDLE_WITHOUT"]
ENV["BUNDLE_WITHOUT"] = "development:test"

puts Bundler::CLI::Common.output_without_groups

ENV["BUNDLE_WITHOUT"] = old_without
