$stdout.sync = true

require "rbconfig"

root = "/tmp/stoned_bundle_platform_#{$$}"
app = File.join(root, "app")
gem_dir = File.join(root, "gem")
[root, app, gem_dir, File.join(gem_dir, "lib")].each { |d| Dir.mkdir(d) unless Dir.exist?(d) }

old_home = ENV["HOME"]
ENV["HOME"] = root

File.write(File.join(gem_dir, "g.gemspec"), <<~GEMSPEC)
  Gem::Specification.new do |s|
    s.name = "g"; s.version = Gem::Version.new("1.0.0")
    s.summary = "g"; s.files = ["lib/g.rb"]; s.require_paths = ["lib"]
  end
GEMSPEC
File.write(File.join(gem_dir, "lib", "g.rb"), "module G; end\n")

File.write(File.join(app, "Gemfile"), <<~GEMFILE)
  source "https://rubygems.org"
  gem "g", path: #{gem_dir.inspect}
GEMFILE

bundle_exe = File.expand_path("exe/bundle", Dir.pwd)
ruby_cmd   = RbConfig.ruby.inspect

Dir.chdir(app) do
  out = `#{ruby_cmd} #{bundle_exe.inspect} platform 2>&1`.chomp
  # Only check that key markers are present (values are platform-dependent)
  puts out.include?("Your platform is:")
  puts out.include?("Your Ruby is:")
  puts out.include?("2.5.0.stoned")
  puts $?.exitstatus

  # With a WITHOUT in config, platform also shows it
  `#{ruby_cmd} #{bundle_exe.inspect} config set WITHOUT development 2>&1`
  out2 = `#{ruby_cmd} #{bundle_exe.inspect} platform 2>&1`.chomp
  puts out2.include?("BUNDLE_WITHOUT")
end

ENV["HOME"] = old_home
system("rm", "-rf", root)
