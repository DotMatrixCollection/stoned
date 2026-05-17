$stdout.sync = true

require "rbconfig"

root = "/tmp/stoned_bundle_path_install_no_tools_#{$$}"
app = File.join(root, "app")
gem_root = File.join(root, "demo_gem")
lib_dir = File.join(gem_root, "lib")
bin_dir = File.join(root, "bin")
[root, app, gem_root, lib_dir, bin_dir].each do |dir|
  Dir.mkdir(dir) unless Dir.exist?(dir)
end

File.write(File.join(gem_root, "demo.gemspec"), <<~GEMSPEC)
  Gem::Specification.new do |s|
    s.name = "demo"
    s.version = Gem::Version.new("0.5.0")
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

File.write(File.join(bin_dir, "ruby"), "#!/bin/sh\nexec #{RbConfig.ruby} \"$@\"\n")
system("chmod", "+x", File.join(bin_dir, "ruby"))

old_path = ENV["PATH"]
ENV["PATH"] = bin_dir
bundle_exe = File.expand_path("exe/bundle", Dir.pwd)

Dir.chdir(app) do
  puts `#{RbConfig.ruby} #{bundle_exe} install 2>&1`.chomp
  puts File.exist?(File.join(app, "Gemfile.lock"))
end

ENV["PATH"] = old_path
system("rm", "-rf", root)
