$stdout.sync = true

require "rbconfig"

root = "/tmp/stoned_bundler_shared_helpers_#{$$}"
app = File.join(root, "app")
[root, app].each do |d|
  Dir.mkdir(d) unless Dir.exist?(d)
end

File.write(File.join(app, "Gemfile"), <<~GEMFILE)
  source "https://rubygems.org"
GEMFILE

old_gemfile = ENV["BUNDLE_GEMFILE"]
old_path = ENV["BUNDLE_PATH"]
ENV["BUNDLE_GEMFILE"] = File.join(app, "Gemfile")
ENV["BUNDLE_PATH"] = "vendor/bundle"

require "bundler/shared_helpers"

puts Bundler::SharedHelpers.root.to_s == app
puts Bundler::SharedHelpers.pwd.to_s == app
puts Bundler::SharedHelpers.default_gemfile.to_s == File.join(app, "Gemfile")
puts Bundler::SharedHelpers.default_lockfile.to_s == File.join(app, "Gemfile.lock")
puts Bundler::SharedHelpers.in_bundle?
puts Bundler::SharedHelpers.default_bundle_dir.to_s.include?(app)
files = Bundler::SharedHelpers.files_in_use
puts files[:gemfile].to_s == File.join(app, "Gemfile")
puts files[:lockfile].to_s == File.join(app, "Gemfile.lock")
Bundler::SharedHelpers.chdir(app) do
  puts Dir.pwd == app
end

ENV["BUNDLE_GEMFILE"] = old_gemfile
ENV["BUNDLE_PATH"] = old_path
system("rm", "-rf", root)
