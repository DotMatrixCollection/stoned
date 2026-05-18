$stdout.sync = true

root = "/tmp/stoned_bundler_runtime_#{$$}"
app = File.join(root, "app")
foo_dir = File.join(root, "foo")
[root, app, foo_dir, File.join(foo_dir, "lib")].each do |d|
  Dir.mkdir(d) unless Dir.exist?(d)
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
File.write(File.join(foo_dir, "lib", "foo.rb"), "RUNTIME_FOO_OK = true\n")

File.write(File.join(app, "Gemfile"), <<~GEMFILE)
  source "https://rubygems.org"
  gem "foo", path: #{foo_dir.inspect}
GEMFILE

File.write(File.join(app, "Gemfile.lock"), <<~LOCK)
  PATH
    remote: #{foo_dir}
    specs:
      foo (1.0.0)

  PLATFORMS
    ruby

  DEPENDENCIES
    foo!

  BUNDLED WITH
     2.5.0.stoned
LOCK

ENV["BUNDLE_GEMFILE"] = File.join(app, "Gemfile")

require "bundler/runtime"

runtime = Bundler.runtime
puts runtime.is_a?(Bundler::Runtime)
puts runtime.definition.is_a?(Bundler::Definition)
puts runtime.settings.is_a?(Bundler::Settings)
puts Bundler.load.is_a?(Bundler::Definition)
runtime.setup
runtime.require(:default)
puts RUNTIME_FOO_OK

ENV["BUNDLE_GEMFILE"] = nil
system("rm", "-rf", root)
