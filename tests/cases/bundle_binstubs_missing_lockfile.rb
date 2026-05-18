$stdout.sync = true

require "rbconfig"

root = "/tmp/stoned_bundle_binstubs_missing_lockfile_#{$$}"
app = File.join(root, "app")
gem_root = File.join(root, "demo_gem")
lib_dir = File.join(gem_root, "lib")
[root, app, gem_root, lib_dir].each { |d| Dir.mkdir(d) unless Dir.exist?(d) }

File.write(File.join(gem_root, "demo.gemspec"), <<~GEMSPEC)
  Gem::Specification.new do |s|
    s.name = "demo"
    s.version = Gem::Version.new("1.0.0")
    s.summary = "demo"
    s.files = ["lib/demo.rb"]
    s.require_paths = ["lib"]
  end
GEMSPEC

File.write(File.join(lib_dir, "demo.rb"), "module Demo; end\n")

File.write(File.join(app, "Gemfile"), <<~GEMFILE)
  source "https://rubygems.org"
  gem "demo", path: #{gem_root.inspect}
GEMFILE

def capture(command)
  output = `#{command} 2>&1`.chomp
  [output, $?.exitstatus]
end

bundle_exe = File.expand_path("exe/bundle", Dir.pwd)

Dir.chdir(app) do
  out, status = capture("#{RbConfig.ruby.inspect} #{bundle_exe.inspect} binstubs demo")
  puts out
  puts status
end

system("rm", "-rf", root)
