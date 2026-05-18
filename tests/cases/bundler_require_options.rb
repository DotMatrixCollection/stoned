$stdout.sync = true

require "rbconfig"

root     = "/tmp/stoned_bundler_require_options_#{$$}"
app      = File.join(root, "app")
gem_home = File.join(root, "gemhome")
foo_dir  = File.join(root, "foo")
bar_dir  = File.join(root, "bar")
baz_dir  = File.join(root, "baz")
[root, app, gem_home, foo_dir, bar_dir, baz_dir].each do |d|
  Dir.mkdir(d) unless Dir.exist?(d)
end
[foo_dir, bar_dir, baz_dir].each do |dir|
  lib = File.join(dir, "lib")
  Dir.mkdir(lib) unless Dir.exist?(lib)
end

File.write(File.join(foo_dir, "foo.gemspec"), <<~GEMSPEC)
  Gem::Specification.new do |s|
    s.name = "foo"
    s.version = Gem::Version.new("1.0.0")
    s.summary = "foo"
    s.files = ["lib/foo.rb"]
    s.require_paths = ["lib"]
  end
GEMSPEC
File.write(File.join(foo_dir, "lib", "foo.rb"), "FOO_AUTO = true\n")

File.write(File.join(bar_dir, "bar.gemspec"), <<~GEMSPEC)
  Gem::Specification.new do |s|
    s.name = "bar"
    s.version = Gem::Version.new("1.0.0")
    s.summary = "bar"
    s.files = ["lib/bar.rb"]
    s.require_paths = ["lib"]
  end
GEMSPEC
File.write(File.join(bar_dir, "lib", "bar.rb"), "BAR_AUTO = true\n")

File.write(File.join(baz_dir, "baz.gemspec"), <<~GEMSPEC)
  Gem::Specification.new do |s|
    s.name = "baz"
    s.version = Gem::Version.new("1.0.0")
    s.summary = "baz"
    s.files = ["lib/baz_core.rb", "lib/baz_extra.rb"]
    s.require_paths = ["lib"]
  end
GEMSPEC
File.write(File.join(baz_dir, "lib", "baz_core.rb"), "BAZ_CORE = true\n")
File.write(File.join(baz_dir, "lib", "baz_extra.rb"), "BAZ_EXTRA = true\n")

File.write(File.join(app, "Gemfile"), <<~GEMFILE)
  source "https://rubygems.org"
  gem "foo", path: #{foo_dir.inspect}
  gem "bar", path: #{bar_dir.inspect}, require: false

  group :tools do
    gem "baz", path: #{baz_dir.inspect}, require: ["baz_core", "baz_extra"]
  end
GEMFILE

File.write(File.join(app, "require_default.rb"), <<~RUBY)
  require "bundler"
  Bundler.require(:default)
  puts(!!defined?(FOO_AUTO))
  puts(defined?(BAR_AUTO).nil?)
  puts(defined?(BAZ_CORE).nil?)
  puts(defined?(BAZ_EXTRA).nil?)
RUBY

File.write(File.join(app, "require_tools.rb"), <<~RUBY)
  require "bundler"
  Bundler.require(:default, :tools)
  puts(!!defined?(FOO_AUTO))
  puts(defined?(BAR_AUTO).nil?)
  puts(!!defined?(BAZ_CORE))
  puts(!!defined?(BAZ_EXTRA))
RUBY

old_gem_home = ENV["GEM_HOME"]
old_home     = ENV["HOME"]
ENV["GEM_HOME"] = gem_home
ENV["HOME"]     = root

ruby_cmd   = RbConfig.ruby.inspect
bundle_exe = File.expand_path("exe/bundle", Dir.pwd)
rubylib    = "RUBYLIB=#{Dir.pwd.inspect}"

Dir.chdir(app) do
  `#{ruby_cmd} #{bundle_exe.inspect} install 2>&1`

  puts "--DEFAULT--"
  puts `#{rubylib} #{ruby_cmd} require_default.rb 2>&1`.chomp

  puts "--TOOLS--"
  puts `#{rubylib} #{ruby_cmd} require_tools.rb 2>&1`.chomp
end

ENV["GEM_HOME"] = old_gem_home
ENV["HOME"]     = old_home
system("rm", "-rf", root)
