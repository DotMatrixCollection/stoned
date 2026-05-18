$stdout.sync = true

require "bundler/definition"
require "bundler/installer"

root = "/tmp/stoned_bundler_installer_#{$$}"
app = File.join(root, "app")
[root, app].each do |d|
  Dir.mkdir(d) unless Dir.exist?(d)
end

File.write(File.join(app, "Gemfile"), "source \"https://rubygems.org\"\n")
File.write(File.join(app, "Gemfile.lock"), "BUNDLED WITH\n   2.5.0.stoned\n")
ENV["BUNDLE_GEMFILE"] = File.join(app, "Gemfile")

definition = Bundler::Definition.build(Bundler.default_gemfile, Bundler.default_lockfile, nil)
installer = Bundler::Installer.install(Bundler.root, definition, path: "vendor/bundle")

puts installer.is_a?(Bundler::Installer)
puts installer.root.to_s == app
puts installer.definition.is_a?(Bundler::Definition)
puts installer.options["path"].nil?
puts installer.options[:path]
puts installer.run

ENV["BUNDLE_GEMFILE"] = nil
system("rm", "-rf", root)
