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
ENV["BUNDLE_GEMFILE"] = File.join(app, "Gemfile")

require "bundler/shared_helpers"

puts Bundler::SharedHelpers.pwd.to_s == app
puts Bundler::SharedHelpers.default_gemfile.to_s == File.join(app, "Gemfile")
puts Bundler::SharedHelpers.default_lockfile.to_s == File.join(app, "Gemfile.lock")
puts Bundler::SharedHelpers.in_bundle?

ENV["BUNDLE_GEMFILE"] = old_gemfile
system("rm", "-rf", root)
