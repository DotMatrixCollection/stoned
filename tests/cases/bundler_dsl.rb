$stdout.sync = true

root = "/tmp/stoned_bundler_dsl_#{$$}"
app = File.join(root, "app")
vendor = File.join(root, "vendor")
[root, app, vendor].each do |d|
  Dir.mkdir(d) unless Dir.exist?(d)
end

File.write(File.join(app, "shared.gemfile"), <<~GEMFILE)
  path "../vendor" do
    gem "gamma"
  end
GEMFILE

File.write(File.join(app, "Gemfile"), <<~GEMFILE)
  source "https://rubygems.org"
  ruby "3.1.0"
  gem "alpha", require: false

  group :tools do
    gem "beta", path: "../vendor", require: ["beta/core", "beta/extra"]
  end

  git "https://github.com/demo/repo.git", branch: "main" do
    gem "delta"
  end

  eval_gemfile "shared.gemfile"
GEMFILE

require "bundler/dsl"
dsl = Bundler::DSL.evaluate(File.join(app, "Gemfile"))
puts dsl.sources.size >= 4
puts dsl.sources.default_source.to_s
puts dsl.dependencies.length
puts dsl.ruby_version.versions[0]
puts dsl.dependencies[0].name
puts dsl.dependencies[0].groups[0]
puts dsl.dependencies[0].autorequire.length
puts dsl.dependencies[1].name
puts dsl.dependencies[1].groups[0]
puts dsl.dependencies[1].source.to_s
puts dsl.dependencies[1].autorequire.join(",")
puts dsl.dependencies[2].name
puts dsl.dependencies[2].source.to_s
puts dsl.dependencies[2].source.branch
puts dsl.dependencies[3].name
puts dsl.dependencies[3].source.to_s

system("rm", "-rf", root)
