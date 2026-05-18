$stdout.sync = true

require "rbconfig"

root = "/tmp/stoned_bundle_config_set_get_#{$$}"
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

def capture(command)
  output = `#{command} 2>&1`.chomp
  [output, $?.exitstatus]
end

bundle_exe = File.expand_path("exe/bundle", Dir.pwd)
ruby_cmd = RbConfig.ruby.inspect

Dir.chdir(app) do
  # list when empty
  out, status = capture("#{ruby_cmd} #{bundle_exe.inspect} config list")
  puts out
  puts status

  # set a key
  puts "--SET--"
  out, status = capture("#{ruby_cmd} #{bundle_exe.inspect} config set WITHOUT development")
  puts out
  puts status

  # get it back
  puts "--GET--"
  out, status = capture("#{ruby_cmd} #{bundle_exe.inspect} config get WITHOUT")
  puts out
  puts status

  # list shows it
  puts "--LIST--"
  out, status = capture("#{ruby_cmd} #{bundle_exe.inspect} config list")
  puts out
  puts status

  # legacy form: get via bundle config KEY
  puts "--LEGACY-GET--"
  out, status = capture("#{ruby_cmd} #{bundle_exe.inspect} config WITHOUT")
  puts out
  puts status

  # unset it
  puts "--UNSET--"
  out, status = capture("#{ruby_cmd} #{bundle_exe.inspect} config unset WITHOUT")
  puts out
  puts status

  # list is empty again
  puts "--LIST-EMPTY--"
  out, status = capture("#{ruby_cmd} #{bundle_exe.inspect} config list")
  puts out
  puts status
end

ENV["HOME"] = old_home
system("rm", "-rf", root)
