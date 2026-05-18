$stdout.sync = true

require "rbconfig"

root = "/tmp/stoned_bundler_definition_#{$$}"
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

require "bundler/definition"

definition = Bundler::Definition.build(Bundler.default_gemfile, Bundler.default_lockfile, nil)
puts definition.gemfile.to_s == File.join(app, "Gemfile")
puts definition.lockfile.to_s == File.join(app, "Gemfile.lock")
puts definition.dependencies[0][:name] == "foo"
puts definition.specs[0].name
puts definition.specs[0].version
puts definition.platforms[0]
puts definition.locked_gems.bundler_version
puts definition.lock.dependencies[0]

ENV["BUNDLE_GEMFILE"] = nil
system("rm", "-rf", root)
