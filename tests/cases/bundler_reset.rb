$stdout.sync = true

require "bundler"
require "bundler/feature_flag"

old_gemfile = ENV["BUNDLE_GEMFILE"]
old_path = ENV["BUNDLE_PATH"]
root = "/tmp/stoned_bundler_reset_#{$$}"
app = File.join(root, "app")
[root, app].each do |d|
  Dir.mkdir(d) unless Dir.exist?(d)
end
File.write(File.join(app, "Gemfile"), "source \"https://rubygems.org\"\n")

ENV["BUNDLE_GEMFILE"] = File.join(app, "Gemfile")
ENV["BUNDLE_PATH"] = "vendor/bundle"

settings1 = Bundler.settings
feature1 = Bundler.feature_flag
puts Bundler.bundle_path.to_s == File.join(app, "vendor", "bundle")
Bundler.reset!
settings2 = Bundler.settings
feature2 = Bundler.feature_flag
puts settings1 == settings2
puts feature1 == feature2
puts Bundler.reset_paths! == Bundler
puts Bundler.use_system_gems? == false

ENV["BUNDLE_GEMFILE"] = old_gemfile
ENV["BUNDLE_PATH"] = old_path
system("rm", "-rf", root)
