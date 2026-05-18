$stdout.sync = true

require "rbconfig"

root     = "/tmp/stoned_bundler_path_helpers_#{$$}"
app      = File.join(root, "app")
gem_home = File.join(root, "gemhome")
[root, app, gem_home].each do |d|
  Dir.mkdir(d) unless Dir.exist?(d)
end

File.write(File.join(app, "Gemfile"), <<~GEMFILE)
  source "https://rubygems.org"
GEMFILE

old_gem_home = ENV["GEM_HOME"]
old_home     = ENV["HOME"]
old_gemfile  = ENV["BUNDLE_GEMFILE"]
ENV["GEM_HOME"] = gem_home
ENV["HOME"]     = root
ENV["BUNDLE_GEMFILE"] = File.join(app, "Gemfile")
ENV["BUNDLE_PATH"] = "vendor/bundle"

require "bundler"

puts Bundler.root.to_s == app
puts Bundler.bundle_path.to_s == File.join(app, "vendor", "bundle")
puts Bundler.app_config_path.to_s == File.join(app, ".bundle")
puts Bundler.app_config.to_s == File.join(app, ".bundle")
puts Bundler.app_cache.to_s == File.join(app, "vendor", "cache")
puts Bundler.user_home.to_s == root
puts Bundler.default_gemfile.dirname.to_s == app
puts Bundler.default_lockfile.dirname.to_s == app

ENV["GEM_HOME"] = old_gem_home
ENV["HOME"]     = old_home
ENV["BUNDLE_GEMFILE"] = old_gemfile
system("rm", "-rf", root)
