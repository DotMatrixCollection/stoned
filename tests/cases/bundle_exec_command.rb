$stdout.sync = true

require "rbconfig"

root = "/tmp/stoned_bundle_exec_command_#{$$}"
app = File.join(root, "app")
gem_root = File.join(root, "demo_gem")
lib_dir = File.join(gem_root, "lib")
bin_dir = File.join(gem_root, "bin")
[root, app, gem_root, lib_dir, bin_dir].each do |dir|
  Dir.mkdir(dir) unless Dir.exist?(dir)
end

File.write(File.join(gem_root, "demo.gemspec"), <<~GEMSPEC)
  Gem::Specification.new do |s|
    s.name = "demo"
    s.version = Gem::Version.new("0.5.0")
    s.summary = "demo"
    s.files = ["lib/demo.rb", "bin/demo-tool"]
    s.require_paths = ["lib"]
    s.executables = ["demo-tool"]
    s.bindir = "bin"
  end
GEMSPEC

File.write(File.join(lib_dir, "demo.rb"), "module Demo\n  VALUE = 42\nend\n")
File.write(File.join(bin_dir, "demo-tool"), <<~RUBY)
  require "demo"
  puts Demo::VALUE
  puts ARGV.inspect
RUBY

File.write(File.join(app, "Gemfile"), <<~GEMFILE)
  source "https://rubygems.org"
  gem "demo", path: #{gem_root.inspect}
GEMFILE

bundle_exe = File.expand_path("exe/bundle", Dir.pwd)

Dir.chdir(app) do
  puts `#{RbConfig.ruby} #{bundle_exe} install 2>&1`.chomp
  puts `#{RbConfig.ruby} #{bundle_exe} exec demo-tool alpha beta 2>&1`.chomp
end

system("rm", "-rf", root)
