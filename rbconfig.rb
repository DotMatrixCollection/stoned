module RbConfig
  RUBY_API_VERSION = "4.0.0"

  def self.root
    @root ||= begin
      dir = File.expand_path(__dir__)
      if File.exist?(File.join(dir, "stoned")) && File.exist?(File.join(dir, "rbconfig.rb"))
        dir
      else
        File.expand_path("../../..", dir)
      end
    end
  end

  def self.bindir
    source_tree = File.join(root, "stoned")
    if File.exist?(source_tree)
      root
    else
      File.join(root, "bin")
    end
  end

  def self.libdir
    source_tree = File.join(root, "rbconfig.rb")
    if File.exist?(source_tree)
      root
    else
      File.join(root, "lib", "ruby", RUBY_API_VERSION)
    end
  end

  CONFIG = {
    "MAJOR" => "4",
    "MINOR" => "0",
    "TEENY" => "0",
    "EXEEXT" => "",
    "ruby_version" => RUBY_API_VERSION,
    "arch" => RUBY_PLATFORM,
    "bindir" => bindir,
    "libdir" => libdir,
    "rubylibdir" => libdir,
    "sitedir" => libdir,
    "sitelibdir" => libdir,
    "vendordir" => libdir,
    "vendorlibdir" => libdir,
    "host_os" => "linux",
  }

  def self.ruby
    source_tree = File.join(root, "stoned")
    if File.exist?(source_tree)
      source_tree
    else
      File.join(bindir, "ruby")
    end
  end
end
