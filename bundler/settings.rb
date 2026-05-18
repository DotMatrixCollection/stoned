require "bundler"

module Bundler
  class Settings
    attr_reader :local_overrides, :command_options

    def initialize(root = nil)
      @root = root
      @temporary = {}
      @local_overrides = {}
      @command_options = {}
    end

    def [](key)
      env_key = normalize_key(key)
      @command_options[env_key] ||
        @temporary[env_key] ||
        @local_overrides[env_key] ||
        ENV[env_key] ||
        merged_config[env_key]
    end

    def key?(key)
      !self[key].nil?
    end

    def all
      merged = merged_config.dup
      ENV.each do |key, value|
        merged[key] = value if key.start_with?("BUNDLE_")
      end
      @local_overrides.each { |key, value| merged[key] = value }
      @temporary.each { |key, value| merged[key] = value }
      @command_options.each { |key, value| merged[key] = value }
      merged
    end

    def path
      self["PATH"]
    end

    def set_local(key, value)
      @local_overrides[normalize_key(key)] = value.to_s
    end

    def set_command_option(key, value)
      @command_options[normalize_key(key)] = value.to_s
    end

    def temporary(options = {})
      previous = @temporary.dup
      options.each do |key, value|
        @temporary[normalize_key(key)] = value.to_s
      end
      return self unless block_given?

      begin
        yield
      ensure
        @temporary = previous
      end
    end

    private

    def merged_config
      read_config(global_config_path).merge(read_config(local_config_path))
    end

    def local_config_path
      File.join(bundle_root, ".bundle", "config")
    end

    def global_config_path
      File.join(ENV["HOME"] || "/", ".bundle", "config")
    end

    def bundle_root
      return @root if @root
      Bundler.root.to_s
    rescue Bundler::GemfileNotFound
      Dir.pwd
    end

    def normalize_key(key)
      raw = key.to_s.upcase
      raw.start_with?("BUNDLE_") ? raw : "BUNDLE_#{raw}"
    end

    def read_config(path)
      return {} unless File.exist?(path)

      config = {}
      File.readlines(path).each do |line|
        stripped = line.chomp
        next if stripped == "---" || stripped.strip.empty?

        if stripped =~ /^(BUNDLE_\w+):\s*"([^"]*)"\s*$/
          config[$1] = $2
        elsif stripped =~ /^(BUNDLE_\w+):\s*(\S+)\s*$/
          config[$1] = $2
        end
      end
      config
    end
  end
end
