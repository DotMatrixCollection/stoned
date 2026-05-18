$stdout.sync = true

require "rbconfig"

root = "/tmp/stoned_bundle_config_group_filter_#{$$}"
app = File.join(root, "app")
core_root = File.join(root, "core_gem")
dev_root  = File.join(root, "dev_gem")
core_lib  = File.join(core_root, "lib")
dev_lib   = File.join(dev_root, "lib")
[root, app, core_root, dev_root, core_lib, dev_lib].each do |dir|
  Dir.mkdir(dir) unless Dir.exist?(dir)
end

File.write(File.join(core_root, "core.gemspec"), <<~GEMSPEC)
  Gem::Specification.new do |s|
    s.name = "core"
    s.version = Gem::Version.new("1.0.0")
    s.summary = "core"
    s.files = ["lib/core.rb"]
    s.require_paths = ["lib"]
  end
GEMSPEC

File.write(File.join(dev_root, "dev.gemspec"), <<~GEMSPEC)
  Gem::Specification.new do |s|
    s.name = "dev"
    s.version = Gem::Version.new("1.0.0")
    s.summary = "dev"
    s.files = ["lib/dev.rb"]
    s.require_paths = ["lib"]
  end
GEMSPEC

File.write(File.join(core_lib, "core.rb"), "module Core; end\n")
File.write(File.join(dev_lib, "dev.rb"), "module Dev; end\n")

File.write(File.join(app, "Gemfile"), <<~GEMFILE)
  source "https://rubygems.org"
  gem "core", path: #{core_root.inspect}
  group :development do
    gem "dev", path: #{dev_root.inspect}
  end
GEMFILE

old_home = ENV["HOME"]
ENV["HOME"] = root  # isolate from user's ~/.bundle/config

bundle_exe = File.expand_path("exe/bundle", Dir.pwd)
ruby_cmd   = RbConfig.ruby.inspect

Dir.chdir(app) do
  # install with all groups
  puts `#{ruby_cmd} #{bundle_exe.inspect} install 2>&1`.chomp
  puts "--LIST-ALL--"
  puts `#{ruby_cmd} #{bundle_exe.inspect} list 2>&1`.chomp

  # set WITHOUT via bundle config (persisted to .bundle/config)
  puts "--SET-WITHOUT--"
  puts `#{ruby_cmd} #{bundle_exe.inspect} config set WITHOUT development 2>&1`.chomp

  # list respects the config
  puts "--LIST-FILTERED--"
  puts `#{ruby_cmd} #{bundle_exe.inspect} list 2>&1`.chomp

  # check also respects the config
  puts "--CHECK--"
  puts `#{ruby_cmd} #{bundle_exe.inspect} check 2>&1`.chomp

  # unset the config
  puts "--UNSET--"
  puts `#{ruby_cmd} #{bundle_exe.inspect} config unset WITHOUT 2>&1`.chomp

  # list shows all again
  puts "--LIST-RESTORED--"
  puts `#{ruby_cmd} #{bundle_exe.inspect} list 2>&1`.chomp
end

ENV["HOME"] = old_home
system("rm", "-rf", root)
