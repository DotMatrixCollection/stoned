$stdout.sync = true

require "rbconfig"

root    = "/tmp/stoned_bundle_exec_ruby_#{$$}"
app     = File.join(root, "app")
gem_dir = File.join(root, "mygem")
lib_dir = File.join(gem_dir, "lib")
[root, app, gem_dir, lib_dir].each do |d|
  Dir.mkdir(d) unless Dir.exist?(d)
end

File.write(File.join(gem_dir, "mygem.gemspec"), <<~GEMSPEC)
  Gem::Specification.new do |s|
    s.name = "mygem"
    s.version = Gem::Version.new("1.0.0")
    s.summary = "mygem"
    s.require_paths = ["lib"]
  end
GEMSPEC
File.write(File.join(lib_dir, "mygem.rb"), "MYGEM_LOADED = true\n")

File.write(File.join(app, "Gemfile"), <<~GEMFILE)
  source "https://rubygems.org"
  gem "mygem", path: #{gem_dir.inspect}
GEMFILE

# A script that uses mygem via require
File.write(File.join(app, "use_mygem.rb"), <<~RUBY)
  require "mygem"
  puts MYGEM_LOADED
  puts ARGV.join(",")
RUBY

old_home = ENV["HOME"]
ENV["HOME"] = root

ruby_cmd   = RbConfig.ruby.inspect
bundle_exe = File.expand_path("exe/bundle", Dir.pwd)
ruby_path  = RbConfig.ruby

Dir.chdir(app) do
  `#{ruby_cmd} #{bundle_exe.inspect} install 2>&1`

  # bundle exec ruby use_mygem.rb alpha beta
  out = `#{ruby_cmd} #{bundle_exe.inspect} exec #{ruby_path.inspect} use_mygem.rb alpha beta 2>&1`.chomp
  puts "--EXEC-RUBY--"
  puts out
end

ENV["HOME"] = old_home
system("rm", "-rf", root)
