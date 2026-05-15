module RbConfig
  CONFIG = {
    "MAJOR" => "4",
    "MINOR" => "0",
    "TEENY" => "0",
    "EXEEXT" => "",
    "ruby_version" => "4.0.0",
    "arch" => RUBY_PLATFORM,
    "bindir" => Dir.pwd,
    "libdir" => Dir.pwd,
    "rubylibdir" => Dir.pwd,
    "sitedir" => Dir.pwd,
    "sitelibdir" => Dir.pwd,
    "vendordir" => Dir.pwd,
    "vendorlibdir" => Dir.pwd,
    "host_os" => "linux",
  }

  def self.ruby
    File.join(Dir.pwd, "stoned")
  end
end
