$stdout.sync = true

root = "/tmp/stoned_bundler_settings_#{$$}"
app = File.join(root, "app")
home = File.join(root, "home")
[root, app, home].each do |d|
  Dir.mkdir(d) unless Dir.exist?(d)
end
File.write(File.join(app, "Gemfile"), "source \"https://rubygems.org\"\n")
Dir.mkdir(File.join(app, ".bundle")) unless Dir.exist?(File.join(app, ".bundle"))
Dir.mkdir(File.join(home, ".bundle")) unless Dir.exist?(File.join(home, ".bundle"))
File.write(File.join(app, ".bundle", "config"), "---\nBUNDLE_RETRY: \"7\"\nBUNDLE_WITHOUT: \"local\"\n")
File.write(File.join(home, ".bundle", "config"), "---\nBUNDLE_PATH: \"global/bundle\"\nBUNDLE_WITHOUT: \"global\"\n")

require "bundler/settings"

old_path = ENV["BUNDLE_PATH"]
old_without = ENV["BUNDLE_WITHOUT"]
old_home = ENV["HOME"]
old_gemfile = ENV["BUNDLE_GEMFILE"]
old_plain = ENV["PLAIN_TEST"]

ENV["BUNDLE_PATH"] = "vendor/bundle"
ENV["BUNDLE_WITHOUT"] = "development:test"
ENV["HOME"] = home
ENV["BUNDLE_GEMFILE"] = File.join(app, "Gemfile")
ENV["PLAIN_TEST"] = "plain"

settings = Bundler.settings
puts settings.is_a?(Bundler::Settings)
puts settings["path"] == "vendor/bundle"
puts settings["WITHOUT"] == "development:test"
puts settings.key?("path")
puts settings.path == "vendor/bundle"
puts settings["retry"] == "7"
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
puts all["BUNDLE_RETRY"] == "2"
puts all["BUNDLE_JOBS"] == "4"
puts all["PLAIN_TEST"].nil?

ENV["BUNDLE_PATH"] = old_path
ENV["BUNDLE_WITHOUT"] = old_without
ENV["HOME"] = old_home
ENV["BUNDLE_GEMFILE"] = old_gemfile
ENV["PLAIN_TEST"] = old_plain
system("rm", "-rf", root)
