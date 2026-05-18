$stdout.sync = true

require "bundler/settings"

old_path = ENV["BUNDLE_PATH"]
old_without = ENV["BUNDLE_WITHOUT"]
old_plain = ENV["PLAIN_TEST"]

ENV["BUNDLE_PATH"] = "vendor/bundle"
ENV["BUNDLE_WITHOUT"] = "development:test"
ENV["PLAIN_TEST"] = "plain"

settings = Bundler.settings
puts settings.is_a?(Bundler::Settings)
puts settings["path"] == "vendor/bundle"
puts settings["WITHOUT"] == "development:test"
puts settings.key?("path")
puts settings.path == "vendor/bundle"
settings.set_local("jobs", 4)
puts settings["jobs"] == "4"
settings.set_command_option("retry", 2)
puts settings["retry"] == "2"
settings.temporary(path: "tmp/bundle") do
  puts settings.path == "tmp/bundle"
end
puts settings.path == "vendor/bundle"
puts settings.local_overrides["BUNDLE_JOBS"] == "4"
all = settings.all
puts all["BUNDLE_PATH"] == "vendor/bundle"
puts all["BUNDLE_WITHOUT"] == "development:test"
puts all["PLAIN_TEST"].nil?

ENV["BUNDLE_PATH"] = old_path
ENV["BUNDLE_WITHOUT"] = old_without
ENV["PLAIN_TEST"] = old_plain
