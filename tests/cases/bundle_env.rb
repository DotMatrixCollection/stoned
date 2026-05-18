$stdout.sync = true

require "rbconfig"

root = "/tmp/stoned_bundle_env_#{$$}"
app = File.join(root, "app")
gem_root = File.join(root, "demo_gem")
lib_dir = File.join(gem_root, "lib")
[root, app, gem_root, lib_dir].each do |dir|
  Dir.mkdir(dir) unless Dir.exist?(dir)
end

File.write(File.join(gem_root, "demo.gemspec"), <<~GEMSPEC)
  Gem::Specification.new do |s|
    s.name = "demo"
    s.version = Gem::Version.new("1.0.0")
    s.summary = "demo"
    s.files = ["lib/demo.rb"]
    s.require_paths = ["lib"]
  end
GEMSPEC

File.write(File.join(lib_dir, "demo.rb"), "module Demo\nend\n")

File.write(File.join(app, "Gemfile"), <<~GEMFILE)
  source "https://rubygems.org"
  gem "demo", path: #{gem_root.inspect}
GEMFILE

old_home = ENV["HOME"]
ENV["HOME"] = root  # isolate from user's ~/.bundle/config

bundle_exe = File.expand_path("exe/bundle", Dir.pwd)
ruby_cmd   = RbConfig.ruby.inspect

Dir.chdir(app) do
  out = `#{ruby_cmd} #{bundle_exe.inspect} env 2>&1`.chomp
  # Only check the deterministic header line and Gemfile section
  lines = out.lines.map(&:chomp)
  lines.each do |line|
    if line.start_with?("## Bundler") ||
       line.start_with?("## Gemfile") ||
       line.start_with?("Bundler version") ||
       line.start_with?("Gemfile:") ||
       line.strip.empty?
      puts line
    end
  end
  puts $?.exitstatus
end

ENV["HOME"] = old_home
system("rm", "-rf", root)
