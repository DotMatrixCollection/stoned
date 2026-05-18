$stdout.sync = true

root = "/tmp/stoned_bundler_dsl_#{$$}"
app = File.join(root, "app")
vendor = File.join(root, "vendor")
[root, app, vendor].each do |d|
  Dir.mkdir(d) unless Dir.exist?(d)
end

File.write(File.join(app, "Gemfile"), <<~GEMFILE)
  source "https://rubygems.org"
  gem "alpha", require: false

  group :tools do
    gem "beta", path: "../vendor", require: ["beta/core", "beta/extra"]
  end
GEMFILE

require "bundler/dsl"
dsl = Bundler::DSL.evaluate(File.join(app, "Gemfile"))
puts dsl.sources[0]
puts dsl.dependencies.length
puts dsl.dependencies[0].name
puts dsl.dependencies[0].groups[0]
puts dsl.dependencies[0].autorequire.length
puts dsl.dependencies[1].name
puts dsl.dependencies[1].groups[0]
puts dsl.dependencies[1].source
puts dsl.dependencies[1].autorequire.join(",")

system("rm", "-rf", root)
