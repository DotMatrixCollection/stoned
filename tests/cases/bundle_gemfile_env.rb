$stdout.sync = true

require "rbconfig"

root = "/tmp/stoned_bundle_gemfile_env_#{$$}"
app = File.join(root, "app")
gem_root = File.join(root, "demo_gem")
lib_dir = File.join(gem_root, "lib")
alt_dir = File.join(root, "alt")
[root, app, gem_root, lib_dir, alt_dir].each { |d| Dir.mkdir(d) unless Dir.exist?(d) }

File.write(File.join(gem_root, "demo.gemspec"), <<~GEMSPEC)
  Gem::Specification.new do |s|
    s.name = "demo"
    s.version = Gem::Version.new("1.0.0")
    s.summary = "demo"
    s.files = ["lib/demo.rb"]
    s.require_paths = ["lib"]
  end
GEMSPEC

File.write(File.join(lib_dir, "demo.rb"), "module Demo\n  VALUE = 55\nend\n")

# primary Gemfile (has demo)
File.write(File.join(app, "Gemfile"), <<~GEMFILE)
  source "https://rubygems.org"
  gem "demo", path: #{gem_root.inspect}
GEMFILE

# alternate Gemfile in a different location (empty)
File.write(File.join(alt_dir, "Gemfile"), <<~GEMFILE)
  source "https://rubygems.org"
GEMFILE

bundle_exe = File.expand_path("exe/bundle", Dir.pwd)
ruby_cmd   = RbConfig.ruby.inspect

# install from the primary Gemfile
Dir.chdir(app) do
  `#{ruby_cmd} #{bundle_exe.inspect} install 2>&1`
end

Dir.chdir(alt_dir) do
  # with no override, list sees only the alt Gemfile
  `#{ruby_cmd} #{bundle_exe.inspect} install 2>&1`
  out = `#{ruby_cmd} #{bundle_exe.inspect} list 2>&1`.chomp
  puts "--ALT-LIST--"
  puts out
end

# BUNDLE_GEMFILE override: point at the primary Gemfile from alt dir
Dir.chdir(alt_dir) do
  old_bg = ENV["BUNDLE_GEMFILE"]
  ENV["BUNDLE_GEMFILE"] = File.join(app, "Gemfile")
  out = `#{ruby_cmd} #{bundle_exe.inspect} list 2>&1`.chomp
  puts "--PRIMARY-LIST--"
  puts out
  ENV["BUNDLE_GEMFILE"] = old_bg
end

system("rm", "-rf", root)
