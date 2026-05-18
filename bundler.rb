require "rubygems"
require "rbconfig"

BUNDLER_RB_DIR = File.dirname(File.expand_path(__FILE__))

module Bundler
  VERSION = "2.5.0.stoned"

  class GemfileNotFound < RuntimeError; end
  class GemNotFound     < RuntimeError; end
  class BundlerError    < RuntimeError; end
  class LockfileError   < BundlerError; end
  class PermissionError < BundlerError; end

  class << self
    def root
      pathname_for(root_path)
    end

    def gemfile
      gemfile_name = ENV["BUNDLE_GEMFILE"]
      if gemfile_name && !gemfile_name.empty?
        File.expand_path(gemfile_name)
      else
        File.join(root_path, "Gemfile")
      end
    end

    def lockfile
      File.join(root_path, "Gemfile.lock")
    end

    def default_gemfile
      pathname_for(gemfile)
    end

    def default_lockfile
      pathname_for(lockfile)
    end

    def setup(*groups)
      require File.join(BUNDLER_RB_DIR, "bundler", "setup")
      self
    end

    def require(*groups)
      # Set up load path first
      setup

      gemfile_path = gemfile
      lockfile_path = lockfile
      return unless File.exist?(gemfile_path)
      return unless File.exist?(lockfile_path)

      requested_groups = groups.flatten.map { |group| group.to_s }
      requested_groups = ["default"] if requested_groups.empty?

      parse_gemfile_dependencies(gemfile_path).each do |dep|
        dep_groups = dep[:groups] || ["default"]
        next if (dep_groups & requested_groups).empty?

        req_setting = dep[:require]
        next if req_setting == false
        if req_setting.is_a?(Array)
          req_setting.each { |r| Kernel.send(:require, r) rescue nil }
        elsif req_setting.is_a?(String)
          Kernel.send(:require, req_setting) rescue nil
        else
          Kernel.send(:require, dep[:name]) rescue nil
        end
      end

      self
    end

    def environment
      self
    end

    def settings
      @settings ||= Bundler::Settings.new
    end

    def current_ruby
      require File.join(BUNDLER_RB_DIR, "bundler", "current_ruby")
      @current_ruby ||= Bundler::CurrentRuby.new
    end

    def rubygems
      require File.join(BUNDLER_RB_DIR, "bundler", "rubygems_integration")
      @rubygems ||= Bundler::RubygemsIntegration.new
    end

    def bundle_path
      configured = settings["PATH"]
      path = if configured && !configured.empty?
        File.expand_path(configured, root_path)
      else
        Gem.home
      end
      pathname_for(path)
    end

    def app_config_path
      pathname_for(File.join(root_path, ".bundle"))
    end

    def app_config
      app_config_path
    end

    def app_cache
      pathname_for(File.join(root_path, "vendor", "cache"))
    end

    def user_home
      pathname_for(ENV["HOME"] || Dir.pwd)
    end

    def with_unbundled_env(&block)
      block.call if block_given?
    end
    alias with_clean_env with_unbundled_env
    alias clean_env with_unbundled_env

    def original_env
      filtered_env
    end

    def unbundled_env
      filtered_env
    end

    def clean_system(*args)
      system(*args)
    end

    def unbundled_system(*args)
      clean_system(*args)
    end

    def ui
      require File.join(BUNDLER_RB_DIR, "bundler", "ui", "shell")
      @ui ||= Bundler::UI::Shell.new
    end

    def feature_flag
      require File.join(BUNDLER_RB_DIR, "bundler", "feature_flag")
      @feature_flag ||= Bundler::FeatureFlag.new(settings)
    end

    def definition
      require File.join(BUNDLER_RB_DIR, "bundler", "definition")
      @definition ||= Bundler::Definition.build(default_gemfile, default_lockfile, nil)
    end

    def runtime
      require File.join(BUNDLER_RB_DIR, "bundler", "runtime")
      @runtime ||= Bundler::Runtime.new(definition, settings)
    end

    def load
      definition
    end

    def locked_gems
      nil
    end

    def use_system_gems?
      !settings.key?("PATH")
    end

    def reset!
      @settings = nil
      @current_ruby = nil
      @ui = nil
      @feature_flag = nil
      @definition = nil
      @runtime = nil
      @rubygems = nil
      self
    end

    def reset_paths!
      reset!
    end

    private

    def root_path
      gemfile_name = ENV["BUNDLE_GEMFILE"]
      if gemfile_name && !gemfile_name.empty?
        return File.dirname(File.expand_path(gemfile_name))
      end

      dir = Dir.pwd
      loop do
        return dir if File.exist?(File.join(dir, "Gemfile"))
        parent = File.dirname(dir)
        break if parent == dir
        dir = parent
      end
      raise GemfileNotFound, "Could not locate Gemfile"
    end

    def pathname_for(path)
      Pathname.new(path)
    rescue Exception
      path
    end

    def filtered_env
      filtered = {}
      ENV.to_h.each do |key, value|
        filtered[key] = value unless key.start_with?("BUNDLE_")
      end
      filtered
    end

    def parse_gemfile_dependencies(gemfile_path)
      deps = []
      group_stack = []

      begin
        File.readlines(gemfile_path).each do |line|
          stripped = line.strip
          next if stripped.empty? || stripped.start_with?("#")

          if stripped =~ /^group\s+(.+?)\s+do\s*$/
            groups = $1.scan(/:([A-Za-z0-9_]+)/).flatten
            group_stack << groups
            next
          end

          if stripped == "end"
            group_stack.pop if group_stack.any?
            next
          end

          next unless stripped =~ /^gem\s+['"]([^'"]+)['"](.*)$/

          gem_name = $1
          rest = $2
          dep_groups = []
          group_stack.each { |groups| dep_groups.concat(groups) }
          dep_groups.concat(extract_inline_groups(rest))
          dep_groups = ["default"] if dep_groups.empty?

          deps << {
            name: gem_name,
            groups: dep_groups.uniq,
            require: extract_require_setting(gem_name, rest),
          }
        end
      rescue Exception
        return []
      end

      deps
    end

    def extract_inline_groups(rest)
      groups = []

      if rest =~ /groups?:\s*\[([^\]]*)\]/
        groups.concat($1.scan(/:([A-Za-z0-9_]+)/).flatten)
      end

      if rest =~ /groups?:\s*:([A-Za-z0-9_]+)/
        groups << $1
      end

      groups
    end

    def extract_require_setting(gem_name, rest)
      return gem_name unless rest =~ /require:\s*(false|true|\[[^\]]*\]|"[^"]*"|'[^']*')/

      token = $1
      return false if token == "false"
      return gem_name if token == "true"
      return token.scan(/['"]([^'"]+)['"]/).flatten if token.start_with?("[")

      token[1...-1]
    end
  end

  class Settings
    def initialize(env = ENV)
      @env = env
      @temporary = {}
      @local_overrides = {}
      @command_options = {}
    end

    def [](key)
      env_key = normalize_key(key)
      return @temporary[env_key] if @temporary.key?(env_key)
      return @command_options[env_key] if @command_options.key?(env_key)
      return @local_overrides[env_key] if @local_overrides.key?(env_key)
      @env[env_key]
    end

    def key?(key)
      env_key = normalize_key(key)
      !self[env_key].nil?
    end

    def all
      snapshot = {}
      @env.to_h.each do |key, value|
        snapshot[key] = value if key.start_with?("BUNDLE_")
      end
      @local_overrides.each { |key, value| snapshot[key] = value }
      @command_options.each { |key, value| snapshot[key] = value }
      @temporary.each { |key, value| snapshot[key] = value }
      snapshot
    end

    def path
      self["PATH"]
    end

    def set_local(key, value)
      @local_overrides[normalize_key(key)] = stringify(value)
    end

    def set_command_option(key, value)
      @command_options[normalize_key(key)] = stringify(value)
    end

    def temporary(overrides = {})
      old_values = {}
      overrides.each do |key, value|
        env_key = normalize_key(key)
        old_values[env_key] = @temporary[env_key]
        @temporary[env_key] = stringify(value)
      end
      yield if block_given?
    ensure
      overrides.each do |key, _value|
        env_key = normalize_key(key)
        if old_values[env_key].nil?
          @temporary.delete(env_key)
        else
          @temporary[env_key] = old_values[env_key]
        end
      end
    end

    def local_overrides
      @local_overrides
    end

    private

    def normalize_key(key)
      str = key.to_s.upcase
      str.start_with?("BUNDLE_") ? str : "BUNDLE_#{str}"
    end

    def stringify(value)
      value.nil? ? nil : value.to_s
    end
  end

  class BundlerUI
    def info(msg, newline = true); end
    def warn(msg, newline = true); end
    def error(msg, newline = true); end
    def debug(msg, newline = true); end
    def confirm(msg, newline = true); end
    def silence; yield; end
  end
end
