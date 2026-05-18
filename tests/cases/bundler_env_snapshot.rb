$stdout.sync = true

require "bundler"

old_gemfile = ENV["BUNDLE_GEMFILE"]
old_custom  = ENV["BUNDLE_TEST"]
old_plain   = ENV["PLAIN_TEST"]

ENV["BUNDLE_GEMFILE"] = "/tmp/demo/Gemfile"
ENV["BUNDLE_TEST"]    = "secret"
ENV["PLAIN_TEST"]     = "plain"

snapshot = Bundler.unbundled_env
puts snapshot["BUNDLE_GEMFILE"].nil?
puts snapshot["BUNDLE_TEST"].nil?
puts snapshot["PLAIN_TEST"] == "plain"

orig = Bundler.original_env
puts orig["BUNDLE_GEMFILE"].nil?
puts orig["BUNDLE_TEST"].nil?
puts orig["PLAIN_TEST"] == "plain"

ENV["BUNDLE_GEMFILE"] = old_gemfile
ENV["BUNDLE_TEST"]    = old_custom
ENV["PLAIN_TEST"]     = old_plain
